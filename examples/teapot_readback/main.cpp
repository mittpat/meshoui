#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <vulkan/vulkan.h>

#include "mo_device.h"
#include "mo_material.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_pipeline.h"
#include "mo_swapchain.h"

#include <linalg.h>

#include <experimental/filesystem>
#include <functional>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fstream>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

static void glfw_error_callback(int, const char* description)
{
    printf("GLFW Error: %s\n", description);
}

static void vk_check_result(VkResult err)
{
    if (err == 0) return;
    printf("[vulkan] Result: %d\n", err);
    if (err < 0)
        abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT objectType, uint64_t, size_t, int32_t, const char*, const char* pMessage, void*)
{
    printf("[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}

#define MoPI (355.f/113)
static constexpr float moDegreesToRadians(float angle)
{
    return angle * MoPI / 180.0f;
}

static float4x4 correction_matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                      { 0.0f,-1.0f, 0.0f, 0.0f },
                                      { 0.0f, 0.0f, 1.0f, 0.0f },
                                      { 0.0f, 0.0f, 0.0f, 1.0f } };
static float4x4 projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), 16/9.f, 0.1f, 1000.f, pos_z, zero_to_one));

struct MoLight
{
    std::string name;
    float4x4    model;
};

struct MoCamera
{
    std::string name;
    float3      position;
    float       pitch, yaw;
    float4x4    model()
    {
        float3 right = mul(rotation_matrix(rotation_quat({0.f,-1.f,0.f}, yaw)), {1.f,0.f,0.f,0.f}).xyz();

        return mul(translation_matrix(position), mul(rotation_matrix(rotation_quat(right, pitch)), rotation_matrix(rotation_quat({0.f,-1.f,0.f}, yaw))));
    }
};

struct MoNode
{
    std::string         name;
    float4x4            model;
    MoMesh              mesh;
    MoMaterial          material;
    std::vector<MoNode> children;
};

struct MoHandles
{
    std::vector<MoMaterial> materials;
    std::vector<MoMesh>     meshes;
};

void moDestroyHandles(MoHandles & handles)
{
    for (auto material : handles.materials)
        moDestroyMaterial(material);
    for (auto mesh : handles.meshes)
        moDestroyMesh(mesh);
}

void parseNodes(const aiScene * scene, const std::vector<MoMaterial> & materials,
                const std::vector<MoMesh> & meshes, aiNode * node, std::vector<MoNode> & nodes)
{
    nodes.push_back({node->mName.C_Str(), transpose(float4x4((float*)&node->mTransformation)),
                     nullptr, nullptr, {}});
    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        nodes.back().children.push_back({scene->mMeshes[node->mMeshes[i]]->mName.C_Str(), identity,
                                    meshes[node->mMeshes[i]],
                                    materials[scene->mMeshes[node->mMeshes[i]]->mMaterialIndex],
                                    {}});
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        parseNodes(scene, materials, meshes, node->mChildren[i], nodes.back().children);
    }
}

void load(const std::string & filename, MoHandles & handles, std::vector<MoNode> & nodes)
{
    if (!filename.empty() && std::filesystem::exists(filename))
    {
        Assimp::Importer importer;
        const aiScene * scene = importer.ReadFile(filename, aiProcess_Debone | aiProcessPreset_TargetRealtime_Fast);
        std::filesystem::path parentdirectory = std::filesystem::path(filename).parent_path();

        std::vector<MoMaterial> materials(scene->mNumMaterials);
        for (uint32_t materialIdx = 0; materialIdx < scene->mNumMaterials; ++materialIdx)
        {
            auto* material = scene->mMaterials[materialIdx];

            MoMaterialCreateInfo materialInfo = {};
            std::vector<std::pair<const char*, float4*>> colorMappings =
               {{std::array<const char*,3>{AI_MATKEY_COLOR_AMBIENT}[0], &materialInfo.colorAmbient},
                {std::array<const char*,3>{AI_MATKEY_COLOR_DIFFUSE}[0], &materialInfo.colorDiffuse},
                {std::array<const char*,3>{AI_MATKEY_COLOR_SPECULAR}[0], &materialInfo.colorSpecular},
                {std::array<const char*,3>{AI_MATKEY_COLOR_EMISSIVE}[0], &materialInfo.colorEmissive}};
            for (auto mapping : colorMappings)
            {
                aiColor3D color(0.f,0.f,0.f);
                scene->mMaterials[materialIdx]->Get(mapping.first, 0, 0, color);
                *mapping.second = {color.r, color.g, color.b, 1.0f};
            }

            std::vector<std::pair<aiTextureType, MoTextureInfo*>> textureMappings =
                {{aiTextureType_AMBIENT, &materialInfo.textureAmbient},
                {aiTextureType_DIFFUSE, &materialInfo.textureDiffuse},
                {aiTextureType_SPECULAR, &materialInfo.textureSpecular},
                {aiTextureType_EMISSIVE, &materialInfo.textureEmissive},
                {aiTextureType_NORMALS, &materialInfo.textureNormal}};
            for (auto mapping : textureMappings)
            {
                aiString path;
                if (AI_SUCCESS == material->GetTexture(mapping.first, 0, &path))
                {
                    std::filesystem::path filename = parentdirectory / path.C_Str();
                    if (std::filesystem::exists(filename))
                    {
                        int x, y, n;
                        mapping.second->pData = stbi_load(filename.c_str(), &x, &y, &n, STBI_rgb_alpha);
                        mapping.second->extent = {(uint32_t)x, (uint32_t)y};
                    }
                }
            }
            moCreateMaterial(&materialInfo, &materials[materialIdx]);
            handles.materials.push_back(materials[materialIdx]);
        }

        std::vector<MoMesh> meshes(scene->mNumMeshes);
        for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
        {
            const auto* mesh = scene->mMeshes[meshIdx];

            std::vector<uint32_t> indices;
            for (uint32_t faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx)
            {
                const auto* face = &mesh->mFaces[faceIdx];
                switch(face->mNumIndices)
                {
                case 1:
                    break;
                case 2:
                    break;
                case 3:
                {
                    for(uint32_t numIndex = 0; numIndex < face->mNumIndices; numIndex++)
                    {
                        indices.push_back(face->mIndices[numIndex]);
                    }
                    break;
                }
                default:
                    break;
                }
            }

            std::vector<float3> vertices, normals, tangents, bitangents;
            std::vector<float2> textureCoords;
            vertices.resize(mesh->mNumVertices);
            textureCoords.resize(mesh->mNumVertices);
            normals.resize(mesh->mNumVertices);
            tangents.resize(mesh->mNumVertices);
            bitangents.resize(mesh->mNumVertices);
            for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
            {
                vertices[vertexIndex] = float3((float*)&mesh->mVertices[vertexIndex]);
                if (mesh->HasTextureCoords(0)) { textureCoords[vertexIndex] = float2((float*)&mesh->mTextureCoords[0][vertexIndex]); }
                if (mesh->mNormals) { normals[vertexIndex] = float3((float*)&mesh->mNormals[vertexIndex]); }
                if (mesh->mTangents) { tangents[vertexIndex] = float3((float*)&mesh->mTangents[vertexIndex]); }
                if (mesh->mBitangents) { bitangents[vertexIndex] = float3((float*)&mesh->mBitangents[vertexIndex]); }
            }

            MoMeshCreateInfo meshInfo = {};
            meshInfo.indexCount = mesh->mNumFaces * 3;
            meshInfo.pIndices = indices.data();
            meshInfo.vertexCount = mesh->mNumVertices;
            meshInfo.pVertices = vertices.data();
            meshInfo.pTextureCoords = textureCoords.data();
            meshInfo.pNormals = normals.data();
            meshInfo.pTangents = tangents.data();
            meshInfo.pBitangents = bitangents.data();
            moCreateMesh(&meshInfo, &meshes[meshIdx]);
            handles.meshes.push_back(meshes[meshIdx]);
        }

        nodes.push_back({std::filesystem::canonical(filename).c_str(), identity,
                         nullptr, nullptr, {}});
        parseNodes(scene, materials, meshes, scene->mRootNode, nodes.back().children);
    }
}

int main(int argc, char** argv)
{
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};
    uint32_t                     frameIndex = 0;

    // Initialization
    int width = 640;
    int height = 480;
    {
        // Create Vulkan instance
        {
            MoInstanceCreateInfo createInfo = {};
#ifndef NDEBUG
            createInfo.debugReport = VK_TRUE;
            createInfo.pDebugReportCallback = vk_debug_report;
#endif
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateInstance(&createInfo, &instance);
        }

        projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

        // Create device
        {
            MoDeviceCreateInfo createInfo = {};
            createInfo.instance = instance;
            createInfo.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            createInfo.pSurfaceFormat = &surfaceFormat;
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateDevice(&createInfo, &device);
        }

        // Create SwapChain, RenderPass, Framebuffer, etc.
        {
            // offscreen
            surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;

            MoSwapChainCreateInfo createInfo = {};
            createInfo.device = device;
            createInfo.surfaceFormat = surfaceFormat;
            createInfo.extent = {(uint32_t)width, (uint32_t)height};
            createInfo.vsync = VK_TRUE;
            createInfo.offscreen = VK_TRUE;
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateSwapChain(&createInfo, &swapChain);
        }
    }

    // Meshoui initialization
    {
        MoInitInfo initInfo = {};
        initInfo.instance = instance;
        initInfo.physicalDevice = device->physicalDevice;
        initInfo.device = device->device;
        initInfo.queueFamily = device->queueFamily;
        initInfo.queue = device->queue;
        initInfo.descriptorPool = device->descriptorPool;
        initInfo.pSwapChainSwapBuffers = swapChain->images;
        initInfo.swapChainSwapBufferCount = MO_FRAME_COUNT;
        initInfo.pSwapChainCommandBuffers = swapChain->frames;
        initInfo.swapChainCommandBufferCount = MO_FRAME_COUNT;
        initInfo.depthBuffer = swapChain->depthBuffer;
        initInfo.swapChainKHR = swapChain->swapChainKHR;
        initInfo.renderPass = swapChain->renderPass;
        initInfo.extent = swapChain->extent;
        initInfo.pCheckVkResultFn = device->pCheckVkResultFn;
        moInit(&initInfo);
    }

    MoHandles handles;
    MoNode root{"__root", identity, nullptr, nullptr, {}};
    MoCamera camera{"__default_camera", {0.f, 10.f, 30.f}, 0.f, 0.f};
    MoLight light{"__default_light", translation_matrix(float3{-300.f, 300.f, 150.f})};

    std::filesystem::path fileToLoad = "resources/teapot.dae";

    // Dome
    MoMesh sphereMesh;
    moDemoSphere(&sphereMesh);
    MoMaterial domeMaterial;
    {
        MoMaterialCreateInfo materialInfo = {};
        materialInfo.colorAmbient = { 0.4f, 0.5f, 0.75f, 1.0f };
        materialInfo.colorDiffuse = { 0.7f, 0.45f, 0.1f, 1.0f };
        moCreateMaterial(&materialInfo, &domeMaterial);
    }
    MoPipeline domePipeline;
    {
        MoPipelineCreateInfo pipelineCreateInfo = {};
        std::vector<char> mo_dome_shader_vert_spv;
        {
            std::ifstream fileStream("dome.vert.spv", std::ifstream::binary);
            mo_dome_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        std::vector<char> mo_dome_shader_frag_spv;
        {
            std::ifstream fileStream("dome.frag.spv", std::ifstream::binary);
            mo_dome_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        pipelineCreateInfo.pVertexShader = (std::uint32_t*)mo_dome_shader_vert_spv.data();
        pipelineCreateInfo.vertexShaderSize = mo_dome_shader_vert_spv.size();
        pipelineCreateInfo.pFragmentShader = (std::uint32_t*)mo_dome_shader_frag_spv.data();
        pipelineCreateInfo.fragmentShaderSize = mo_dome_shader_frag_spv.size();
        pipelineCreateInfo.flags = MO_PIPELINE_FEATURE_NONE;
        moCreatePipeline(&pipelineCreateInfo, &domePipeline);
    }

    std::vector<std::uint8_t> readback;

    // one frame
    {
        if (!fileToLoad.empty())
        {
            load(fileToLoad, handles, root.children);
            fileToLoad = "";
        }

        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moPipelineOverride(domePipeline);
        moBegin(frameIndex);

        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.position;
            moSetLight(&uni);
        }
        {
            float4x4 view = inverse(camera.model());
            view.w = float4(0,0,0,1);

            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = view;
            {
                pmv.model = identity;
                moSetPMV(&pmv);
                moBindMaterial(domeMaterial);
                moDrawMesh(sphereMesh);
            }
        }
        moPipelineOverride();
        moBegin(frameIndex);
        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.model().w.xyz();
            moSetLight(&uni);
        }
        {
            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = inverse(camera.model());
            std::function<void(const MoNode &, const float4x4 &)> draw = [&](const MoNode & node, const float4x4 & model)
            {
                if (node.material && node.mesh)
                {
                    moBindMaterial(node.material);
                    pmv.model = model;
                    moSetPMV(&pmv);
                    moDrawMesh(node.mesh);
                }
                for (const MoNode & child : node.children)
                {
                    draw(child, mul(model, child.model));
                }
            };
            draw(root, root.model);
        }

        // Frame end
        moEndSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);

        // offscreen
        readback.resize(width * height * 4);
        moFramebufferReadback(swapChain->images[0].back, {std::uint32_t(width), std::uint32_t(height)}, readback.data(), readback.size(), swapChain->frames[0].pool);
    }

    // Dome
    moDestroyPipeline(domePipeline);
    moDestroyMaterial(domeMaterial);

    // Meshoui cleanup
    moDestroyHandles(handles);
    moDestroyMesh(sphereMesh);

    // Cleanup
    moShutdown();
    moDestroySwapChain(device, swapChain);
    moDestroyDevice(device);
    moDestroyInstance(instance);

    // offscreen
    char outputFilename[256];
    snprintf(outputFilename, 256, "%s_%d_readback.png", argv[0], int(time(0)));
    stbi_write_png(outputFilename, width, height, 4, readback.data(), 4 * width);
    printf("saved output as %s\n", outputFilename);

    return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2018 Patrick Pelletier
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
