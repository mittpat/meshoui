#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "mo_device.h"
#include "mo_example_utils.h"
#include "mo_glfw_utils.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_pipeline.h"
#include "mo_pipeline_utils.h"
#include "mo_swapchain.h"

#include <filesystem>

using namespace std::filesystem;
using namespace linalg;
using namespace linalg::aliases;

int main(int argc, char** argv)
{
    printf("[cube_passthrough] main() starting\n");
    fflush(stdout);

    GLFWwindow*                  window = nullptr;
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceKHR                 surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};

    // Initialization
    {
        printf("[cube_passthrough] Initializing GLFW...\n");
        fflush(stdout);

        glfwSetErrorCallback(moGlfwErrorCallback);
        glfwInit();

        printf("[cube_passthrough] GLFW initialized\n");
        fflush(stdout);
        int width, height;
        width = 1920 / 2;
        height = 1080 / 2;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, std::filesystem::path(argv[0]).stem().c_str(), nullptr, nullptr);

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

        // Create device
        {
            printf("[cube_passthrough] About to create device...\n");
            fflush(stdout);

            MoDeviceCreateInfo info = {};
            info.instance = instance;
            info.surface = surface;
            VkFormat requestFormats[4] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            info.pRequestFormats = requestFormats;
            info.requestFormatsCount = 4;
            info.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            info.pSurfaceFormat = &surfaceFormat;
            info.pCheckVkResultFn = moVkCheckResult;

            printf("[cube_passthrough] Calling moCreateDevice...\n");
            fflush(stdout);

            moCreateDevice(&info, &device);

            printf("[cube_passthrough] Device created: %p\n", (void*)device);
            fflush(stdout);
        }

        // Create SwapChain, RenderPass, Framebuffer, etc.
        {
            printf("[cube_passthrough] About to create swapchain...\n");
            fflush(stdout);

            MoSwapChainCreateInfo info = {};
            info.surface = surface;
            info.surfaceFormat = surfaceFormat;
            info.extent = {(uint32_t)width, (uint32_t)height};
            info.vsync = VK_TRUE;
            info.pCheckVkResultFn = moVkCheckResult;

            printf("[cube_passthrough] Calling moCreateSwapChain...\n");
            fflush(stdout);

            moCreateSwapChain(&info, &swapChain);

            printf("[cube_passthrough] SwapChain created: %p\n", (void*)swapChain);
            fflush(stdout);
        }
    }

    printf("[cube_passthrough] About to create pipeline layout...\n");
    fflush(stdout);

    MoPipelineLayout pipelineLayout;
    moCreatePipelineLayout(&pipelineLayout);

    printf("[cube_passthrough] Pipeline layout created: %p\n", (void*)pipelineLayout);
    fflush(stdout);

    // Passthrough
    printf("[cube_passthrough] About to create pipeline...\n");
    fflush(stdout);

    VkPipeline passthroughPipeline;
    moCreatePipeline(swapChain->renderPass, pipelineLayout->pipelineLayout, "passthrough.glsl", &passthroughPipeline, MO_PIPELINE_FEATURE_NONE);

    printf("[cube_passthrough] Pipeline created: %p\n", (void*)passthroughPipeline);
    fflush(stdout);

    printf("[cube_passthrough] About to create demo cube...\n");
    fflush(stdout);

    MoMesh cubeMesh;
    moCreateDemoCube(&cubeMesh, float3(0.5,0.5,0.5));

    printf("[cube_passthrough] Demo cube created: %p\n", (void*)cubeMesh);
    fflush(stdout);

    printf("[cube_passthrough] About to enter main loop\n");
    fflush(stdout);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        printf("[cube_passthrough] Main loop iteration starting\n");
        fflush(stdout);

        glfwPollEvents();

        printf("[cube_passthrough] About to begin swapchain\n");
        fflush(stdout);

        // Frame begin
        MoCommandBuffer currentCommandBuffer;
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &currentCommandBuffer, &imageAcquiredSemaphore);

        printf("[cube_passthrough] Swapchain begun\n");
        fflush(stdout);

        moBeginRenderPass(swapChain, currentCommandBuffer);

        printf("[cube_passthrough] Render pass begun\n");
        fflush(stdout);

        moBindPipeline(currentCommandBuffer.buffer, passthroughPipeline, pipelineLayout->pipelineLayout, pipelineLayout->descriptorSet[swapChain->currentFrame]);

        printf("[cube_passthrough] Pipeline bound\n");
        fflush(stdout);

        moDrawMesh(currentCommandBuffer.buffer, cubeMesh);

        printf("[cube_passthrough] Mesh drawn\n");
        fflush(stdout);

        // Frame end
        printf("[cube_passthrough] About to end swapchain\n");
        fflush(stdout);

        VkResult err = moEndSwapChain(swapChain, &imageAcquiredSemaphore);

        printf("[cube_passthrough] Swapchain ended, err=%d\n", err);
        fflush(stdout);
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
    moDestroyMesh(cubeMesh);
    vkDestroyPipeline(device->device, passthroughPipeline, VK_NULL_HANDLE);
    passthroughPipeline = VK_NULL_HANDLE;
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
