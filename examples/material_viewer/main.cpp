#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "mo_device.h"
#include "mo_example_utils.h"
#include "mo_glfw_utils.h"
#include "mo_material.h"
#include "mo_material_utils.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_node.h"
#include "mo_pipeline.h"
#include "mo_pipeline_utils.h"
#include "mo_swapchain.h"

#include <linalg.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <experimental/filesystem>
#include <fstream>
#include <functional>

#include <rapidjson/document.h>
#include <rapidjson/encodedstream.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

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
    GLFWwindow*                  window = nullptr;
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceKHR                 surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};

    MoInputs                     inputs = {};

    // Initialization
    {
        glfwSetErrorCallback(moGlfwErrorCallback);
        glfwInit();
        int width, height;
#ifndef NDEBUG
        width = 1920 / 2;
        height = 1080 / 2;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, std::filesystem::path(argv[0]).stem().c_str(), nullptr, nullptr);
#else
        {
            const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            width = mode->width;
            height = mode->height;
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        window = glfwCreateWindow(width, height, "MeshouiView", nullptr, nullptr);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, width, height, GLFW_DONT_CARE);
#endif
        moInitInputs(window, &inputs);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

        // Create Vulkan instance
        {
            MoInstanceCreateInfo info = {};
            info.pExtensions = glfwGetRequiredInstanceExtensions(&info.extensionsCount);
#ifndef NDEBUG
            info.debugReport = VK_TRUE;
            info.pDebugReportCallback = moVkDebugReport;
#endif
            info.pCheckVkResultFn = moVkCheckResult;
            moCreateInstance(&info, &instance);
        }

        // Create Window Surface
        VkResult err = glfwCreateWindowSurface(instance, window, VK_NULL_HANDLE, &surface);
        moVkCheckResult(err);

        // Create Framebuffers
        width = height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwPostEmptyEvent();
            glfwWaitEvents();
        }

        projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

        // Create device
        {
            MoDeviceCreateInfo info = {};
            info.instance = instance;
            info.surface = surface;
            VkFormat requestFormats[4] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            info.pRequestFormats = requestFormats;
            info.requestFormatsCount = 4;
            info.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            info.pSurfaceFormat = &surfaceFormat;
            info.pCheckVkResultFn = moVkCheckResult;
            moCreateDevice(&info, &device);
        }

        // Create SwapChain, RenderPass, Framebuffer, etc.
        {
            MoSwapChainCreateInfo info = {};
            info.surface = surface;
            info.surfaceFormat = surfaceFormat;
            info.extent = {(uint32_t)width, (uint32_t)height};
            info.vsync = VK_TRUE;
            info.pCheckVkResultFn = moVkCheckResult;
            moCreateSwapChain(&info, &swapChain);
        }
    }

    MoPipelineLayout pipelineLayout;
    moCreatePipelineLayout(&pipelineLayout);

    // Phong
    VkPipeline phongPipeline;
    moCreatePipeline(swapChain->renderPass, pipelineLayout->pipelineLayout, "phong.glsl", &phongPipeline);

    // Dome
    VkPipeline domePipeline;
    moCreatePipeline(swapChain->renderPass, pipelineLayout->pipelineLayout, "dome.glsl", &domePipeline, MO_PIPELINE_FEATURE_NONE);

    MoMesh sphereMesh;
    moCreateDemoSphere(&sphereMesh);
    MoMaterial domeMaterial;
    {
        MoMaterialCreateInfo info = {};
        info.colorAmbient = { 0.4f, 0.5f, 0.75f, 1.0f };
        info.colorDiffuse = { 0.7f, 0.45f, 0.1f, 1.0f };
        info.commandBuffer = swapChain->frames[0].buffer;
        info.commandPool = swapChain->frames[0].pool;
        moCreateMaterial(&info, &domeMaterial);
        moRegisterMaterial(pipelineLayout, domeMaterial);
    }
    MoCamera camera{"__default_camera", {0.f, 0.f, 5.f}, 0.f, 0.f};
    MoLight light{"__default_light", translation_matrix(float3{-300.f, 300.f, 150.f})};

    stbi_set_flip_vertically_on_load(1);

    MoMaterial bricksMaterial;
    {
        std::ifstream fileStream("resources/bricks.json");
        rapidjson::IStreamWrapper fileStreamWrapper(fileStream);
        rapidjson::Document document;
        document.ParseStream(fileStreamWrapper);

        MoMaterialCreateInfo info = {};
        info.commandBuffer = swapChain->frames[0].buffer;
        info.commandPool = swapChain->frames[0].pool;
        info.colorAmbient = { 0.1f, 0.1f, 0.1f, 1.0f };
        info.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
        info.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
        info.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
        int x, y, n;
        info.textureDiffuse.pData = stbi_load((std::filesystem::path("resources") / document["diffuse"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureDiffuse.extent = {uint32_t(x),uint32_t(y)};
        info.textureNormal.pData = stbi_load((std::filesystem::path("resources") / document["normal"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureNormal.extent = {uint32_t(x),uint32_t(y)};
        moCreateMaterial(&info, &bricksMaterial);
        moRegisterMaterial(pipelineLayout, bricksMaterial);
    }

    MoMaterial marbleMaterial;
    {
        std::ifstream fileStream("resources/marble.json");
        rapidjson::IStreamWrapper fileStreamWrapper(fileStream);
        rapidjson::Document document;
        document.ParseStream(fileStreamWrapper);

        MoMaterialCreateInfo info = {};
        info.commandBuffer = swapChain->frames[0].buffer;
        info.commandPool = swapChain->frames[0].pool;
        info.colorAmbient = { 0.1f, 0.1f, 0.1f, 1.0f };
        info.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
        info.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
        info.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
        int x, y, n;
        info.textureDiffuse.pData = stbi_load((std::filesystem::path("resources") / document["diffuse"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureDiffuse.extent = {uint32_t(x),uint32_t(y)};
        info.textureNormal.pData = stbi_load((std::filesystem::path("resources") / document["normal"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureNormal.extent = {uint32_t(x),uint32_t(y)};
        info.textureSpecular.pData = stbi_load((std::filesystem::path("resources") / document["specular"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureSpecular.extent = {uint32_t(x),uint32_t(y)};
        info.textureEmissive.pData = stbi_load((std::filesystem::path("resources") / document["emissive"].GetString()).c_str(), &x, &y, &n, STBI_rgb_alpha);
        info.textureEmissive.extent = {uint32_t(x),uint32_t(y)};
        moCreateMaterial(&info, &marbleMaterial);
        moRegisterMaterial(pipelineLayout, marbleMaterial);
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        moPollMouse(window);

        moInputTransform(&inputs, &camera);

        // Frame begin
        MoCommandBuffer currentCommandBuffer;
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &currentCommandBuffer, &imageAcquiredSemaphore);
        moBeginRenderPass(swapChain, currentCommandBuffer);
        {
            MoUniform uni = {};
            uni.projection = projection_matrix;
            uni.view = inverse(camera.model());
            uni.light = light.model.w.xyz();
            uni.camera = camera.position;
            moUploadBuffer(pipelineLayout->uniformBuffer[swapChain->frameIndex], sizeof(MoUniform), &uni);
        }
        moBindPipeline(currentCommandBuffer.buffer, domePipeline, pipelineLayout->pipelineLayout, pipelineLayout->descriptorSet[swapChain->frameIndex]);
        {
            MoPushConstant pmv = {};
            pmv.model = identity;
            moBindMaterial(currentCommandBuffer.buffer, domeMaterial, pipelineLayout->pipelineLayout);
            vkCmdPushConstants(currentCommandBuffer.buffer, pipelineLayout->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), &pmv);
            moDrawMesh(currentCommandBuffer.buffer, sphereMesh);
        }
        moBindPipeline(currentCommandBuffer.buffer, phongPipeline, pipelineLayout->pipelineLayout, pipelineLayout->descriptorSet[swapChain->frameIndex]);
        {
            MoPushConstant pmv = {};
            pmv.model = identity;
            moBindMaterial(currentCommandBuffer.buffer, bricksMaterial, pipelineLayout->pipelineLayout);
            vkCmdPushConstants(currentCommandBuffer.buffer, pipelineLayout->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), &pmv);
            moDrawMesh(currentCommandBuffer.buffer, sphereMesh);

            pmv.model = translation_matrix(float3(-5,0,0));
            moBindMaterial(currentCommandBuffer.buffer, marbleMaterial, pipelineLayout->pipelineLayout);
            vkCmdPushConstants(currentCommandBuffer.buffer, pipelineLayout->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), &pmv);
            moDrawMesh(currentCommandBuffer.buffer, sphereMesh);
        }

        // Frame end
        VkResult err = moEndSwapChain(swapChain, &imageAcquiredSemaphore);
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            int width = 0, height = 0;
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(window, &width, &height);
                glfwPostEmptyEvent();
                glfwWaitEvents();
            }

            // in case the window was resized
            projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

            MoSwapChainRecreateInfo recreateInfo = {};
            recreateInfo.surface = surface;
            recreateInfo.surfaceFormat = surfaceFormat;
            recreateInfo.extent = {(uint32_t)width, (uint32_t)height};
            recreateInfo.vsync = VK_TRUE;
            moRecreateSwapChain(&recreateInfo, swapChain);
            err = VK_SUCCESS;
        }
        moVkCheckResult(err);
    }

    // Meshoui cleanup
    moDestroyMaterial(marbleMaterial);
    moDestroyMaterial(bricksMaterial);
    moDestroyMaterial(domeMaterial);
    moDestroyMesh(sphereMesh);
    vkDestroyPipeline(device->device, domePipeline, VK_NULL_HANDLE);
    domePipeline = VK_NULL_HANDLE;
    vkDestroyPipeline(device->device, phongPipeline, VK_NULL_HANDLE);
    phongPipeline = VK_NULL_HANDLE;
    moDestroyPipelineLayout(pipelineLayout);

    // Cleanup
    moDestroySwapChain(swapChain);
    vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
    moDestroyDevice(device);
    moDestroyInstance(instance);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

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
