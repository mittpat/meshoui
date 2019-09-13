#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "mo_device.h"
#include "mo_example_utils.h"
#include "mo_material.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_node.h"
#include "mo_pipeline.h"
#include "mo_pipeline_utils.h"
#include "mo_swapchain.h"

#include <linalg.h>

#include <experimental/filesystem>
#include <functional>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

struct MoInputs
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool forward;
    bool backward;
    bool leftButton;
    bool rightButton;
    double xpos, ypos;
    double dxpos, dypos;
};

static void glfwKeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_Q:
            inputs->up = true;
            break;
        case GLFW_KEY_E:
            inputs->down = true;
            break;
        case GLFW_KEY_W:
            inputs->forward = true;
            break;
        case GLFW_KEY_A:
            inputs->left = true;
            break;
        case GLFW_KEY_S:
            inputs->backward = true;
            break;
        case GLFW_KEY_D:
            inputs->right = true;
            break;
        }
    }
    if (action == GLFW_RELEASE)
    {
        switch (key)
        {
        case GLFW_KEY_Q:
            inputs->up = false;
            break;
        case GLFW_KEY_E:
            inputs->down = false;
            break;
        case GLFW_KEY_W:
            inputs->forward = false;
            break;
        case GLFW_KEY_A:
            inputs->left = false;
            break;
        case GLFW_KEY_S:
            inputs->backward = false;
            break;
        case GLFW_KEY_D:
            inputs->right = false;
            break;
        }
    }
}

static void glfwMouseCallback(GLFWwindow *window, int button, int action, int /*mods*/)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            inputs->leftButton = true;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            inputs->rightButton = true;
            break;
        }
    }
    else if (action == GLFW_RELEASE)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            inputs->leftButton = false;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            inputs->rightButton = false;
            break;
        }
    }
}

static void glfwPollMouse(GLFWwindow *window)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    inputs->dxpos = xpos - inputs->xpos;
    inputs->dypos = ypos - inputs->ypos;
    inputs->xpos = xpos;
    inputs->ypos = ypos;
}

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
    uint32_t                     frameIndex = 0;

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
        glfwGetCursorPos(window, &inputs.xpos, &inputs.ypos);
        glfwSetWindowUserPointer(window, &inputs);
        glfwSetKeyCallback(window, glfwKeyCallback);
        glfwSetMouseButtonCallback(window, glfwMouseCallback);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

        // Create Vulkan instance
        {
            MoInstanceCreateInfo createInfo = {};
            createInfo.pExtensions = glfwGetRequiredInstanceExtensions(&createInfo.extensionsCount);
#ifndef NDEBUG
            createInfo.debugReport = VK_TRUE;
            createInfo.pDebugReportCallback = moVkDebugReport;
#endif
            createInfo.pCheckVkResultFn = moVkCheckResult;
            moCreateInstance(&createInfo, &instance);
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
            MoDeviceCreateInfo createInfo = {};
            createInfo.instance = instance;
            createInfo.surface = surface;
            VkFormat requestFormats[4] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            createInfo.pRequestFormats = requestFormats;
            createInfo.requestFormatsCount = 4;
            createInfo.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            createInfo.pSurfaceFormat = &surfaceFormat;
            createInfo.pCheckVkResultFn = moVkCheckResult;
            moCreateDevice(&createInfo, &device);
        }

        // Create SwapChain, RenderPass, Framebuffer, etc.
        {
            MoSwapChainCreateInfo createInfo = {};
            createInfo.device = device;
            createInfo.surface = surface;
            createInfo.surfaceFormat = surfaceFormat;
            createInfo.extent = {(uint32_t)width, (uint32_t)height};
            createInfo.vsync = VK_TRUE;
            createInfo.pCheckVkResultFn = moVkCheckResult;
            moCreateSwapChain(&createInfo, &swapChain);
        }
    }

    // Meshoui initialization
    {
        MoInitInfo info = {};
        info.instance = instance;
        info.physicalDevice = device->physicalDevice;
        info.device = device->device;
        info.queueFamily = device->queueFamily;
        info.queue = device->queue;
        info.descriptorPool = device->descriptorPool;
        info.pSwapChainSwapBuffers = swapChain->images;
        info.swapChainSwapBufferCount = MO_FRAME_COUNT;
        info.pSwapChainCommandBuffers = swapChain->frames;
        info.swapChainCommandBufferCount = MO_FRAME_COUNT;
        info.depthBuffer = swapChain->depthBuffer;
        info.swapChainKHR = swapChain->swapChainKHR;
        info.renderPass = swapChain->renderPass;
        info.extent = swapChain->extent;
        info.pCheckVkResultFn = device->pCheckVkResultFn;
        moInit(&info);
    }

    // Phong
    MoPipeline phongPipeline;
    moCreatePhongPipeline(&phongPipeline);

    // Dome
    MoPipeline domePipeline;
    moCreateDomePipeline(&domePipeline);

    MoMesh sphereMesh;
    moCreateDemoSphere(&sphereMesh);
    MoMaterial domeMaterial;
    {
        MoMaterialCreateInfo info = {};
        info.colorAmbient = { 0.4f, 0.5f, 0.75f, 1.0f };
        info.colorDiffuse = { 0.7f, 0.45f, 0.1f, 1.0f };
        moCreateMaterial(&info, &domeMaterial);
        moRegisterMaterial(domePipeline, domeMaterial);
    }
    MoCamera camera{"__default_camera", {0.f, 10.f, 30.f}, 0.f, 0.f};
    MoLight light{"__default_light", translation_matrix(float3{-300.f, 300.f, 150.f})};
    MoScene scene = {};

    std::filesystem::path fileToLoad = "resources/teapot.dae";

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glfwPollMouse(window);

        if (!fileToLoad.empty())
        {
            moCreateScene(fileToLoad.c_str(), &scene);
            for (std::uint32_t i = 0; i < scene->materialCount; ++i)
            {
                moRegisterMaterial(phongPipeline, scene->pMaterials[i]);
            }
            fileToLoad = "";
        }

        {
            const float speed = 0.5f;
            float3 forward = mul(camera.model(), {0.f,0.f,-1.f,0.f}).xyz();
            float3 up = mul(camera.model(), {0.f,1.f,0.f,0.f}).xyz();
            float3 right = mul(camera.model(), {1.f,0.f,0.f,0.f}).xyz();
            if (inputs.up) camera.position += up * speed;
            if (inputs.down) camera.position -= up * speed;
            if (inputs.forward) camera.position += forward * speed;
            if (inputs.backward) camera.position -= forward * speed;
            if (inputs.left) camera.position -= right * speed;
            if (inputs.right) camera.position += right * speed;
            if (inputs.leftButton)
            {
                camera.yaw += inputs.dxpos * 0.005;
                camera.pitch -= inputs.dypos * 0.005;
            }
        }

        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moBegin(frameIndex, domePipeline);

        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.position;
            moUploadBuffer(device, domePipeline->uniformBuffer[frameIndex], sizeof(MoUniform), &uni);
        }
        {
            float4x4 view = inverse(camera.model());
            view.w = float4(0,0,0,1);

            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = view;
            {
                pmv.model = identity;
                vkCmdPushConstants(swapChain->frames[frameIndex].buffer, domePipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), &pmv);
                moBindMaterial(domeMaterial, domePipeline->pipelineLayout);
                moDrawMesh(sphereMesh);
            }
        }
        moBegin(frameIndex, phongPipeline);
        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.model().w.xyz();
            moUploadBuffer(device, phongPipeline->uniformBuffer[frameIndex], sizeof(MoUniform), &uni);
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
                    moBindMaterial(node->material, phongPipeline->pipelineLayout);
                    pmv.model = model;
                    vkCmdPushConstants(swapChain->frames[frameIndex].buffer, phongPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), &pmv);
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
        VkResult err = moEndSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
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
    moDestroyScene(scene);
    moDestroyMaterial(domeMaterial);
    moDestroyMesh(sphereMesh);
    moDestroyPipeline(domePipeline);
    moDestroyPipeline(phongPipeline);

    // Cleanup
    moShutdown();
    moDestroySwapChain(device, swapChain);
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
