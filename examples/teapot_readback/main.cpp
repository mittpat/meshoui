#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <vulkan/vulkan.h>

#include "mo_device.h"
#include "mo_example_utils.h"
#include "mo_material.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_node.h"
#include "mo_pipeline.h"
#include "mo_swapchain.h"

#include <linalg.h>

#include <experimental/filesystem>
#include <functional>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fstream>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

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
            createInfo.pDebugReportCallback = moVkDebugReport;
#endif
            createInfo.pCheckVkResultFn = moVkCheckResult;
            moCreateInstance(&createInfo, &instance);
        }

        projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

        // Create device
        {
            MoDeviceCreateInfo createInfo = {};
            createInfo.instance = instance;
            createInfo.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            createInfo.pSurfaceFormat = &surfaceFormat;
            createInfo.pCheckVkResultFn = moVkCheckResult;
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
            createInfo.pCheckVkResultFn = moVkCheckResult;
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

    MoScene scene = {};

    // one frame
    {
        if (!fileToLoad.empty())
        {
            moCreateScene(fileToLoad.c_str(), &scene);
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
        if (scene)
        {
            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = inverse(camera.model());
            std::function<void(MoNode, const float4x4 &)> draw = [&](MoNode node, const float4x4 & model)
            {
                if (node->material && node->mesh)
                {
                    moBindMaterial(node->material);
                    pmv.model = model;
                    moSetPMV(&pmv);
                    moDrawMesh(node->mesh);
                }
                for (std::uint32_t i = 0; i < node->nodeCount; ++i)
                {
                    draw(node->pNodes[i], mul(model, node->pNodes[i]->model));
                }
            };
            draw(scene->root, scene->root->model);
        }

        // Frame end
        moEndSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);

        // offscreen
        readback.resize(width * height * 4);
        moFramebufferReadback(swapChain->images[0].back, {std::uint32_t(width), std::uint32_t(height)}, readback.data(), readback.size(), swapChain->frames[0].pool);
    }

    // Meshoui cleanup
    moDestroyScene(scene);
    moDestroyPipeline(domePipeline);
    moDestroyMaterial(domeMaterial);
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
