#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

#include "mo_device.h"
#include "mo_example_utils.h"
#include "mo_mesh.h"
#include "mo_mesh_utils.h"
#include "mo_pipeline.h"
#include "mo_pipeline_utils.h"
#include "mo_swapchain.h"

#include <algorithm>
#include <experimental/filesystem>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

void moGUIInit(GLFWwindow *pWindow, VkDevice device, VkRenderPass renderPass, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue, ImGui_ImplVulkan_InitInfo *pInfo)
{
    // IMGUI initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(pWindow, true);
    ImGui_ImplVulkan_Init(pInfo, renderPass);

    ImGui::StyleColorsDark();

    VkResult err = vkResetCommandPool(device, commandPool, 0);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(commandBuffer, &begin_info);

    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &commandBuffer;
    err = vkEndCommandBuffer(commandBuffer);
    err = vkQueueSubmit(queue, 1, &end_info, VK_NULL_HANDLE);

    err = vkDeviceWaitIdle(device);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void moGUIDrawBegin()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
}

bool moGUIFileBrowser(std::experimental::filesystem::path *pCurrentDirectory, std::experimental::filesystem::path *pFileToLoad, const std::vector<std::string>& allowedExtensions = {})
{
    if (!std::filesystem::equivalent(*pCurrentDirectory, *pCurrentDirectory / ".."))
    {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(6/7.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(6/7.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(6/7.0f, 0.8f, 0.8f));
        if (ImGui::Button(".."))
        {
            *pCurrentDirectory = std::filesystem::canonical(*pCurrentDirectory / "..");
        }
        ImGui::PopStyleColor(3);
    }
    std::vector<std::filesystem::path> paths;
    std::filesystem::directory_iterator position(*pCurrentDirectory);
    paths.insert(paths.end(), std::filesystem::begin(position), std::filesystem::end(position));
    std::sort(paths.begin(), paths.end(), [](const std::filesystem::path & a, const std::filesystem::path & b)
    {
        if (std::filesystem::is_directory(a) && !std::filesystem::is_directory(b))
            return true;
        if (!std::filesystem::is_directory(a) && std::filesystem::is_directory(b))
            return false;
        return a < b;
    });
    for (auto & p: paths)
    {
        if (p.filename().c_str()[0] != '.')
        {
            if (std::filesystem::is_directory(p))
            {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(5/7.0f, 0.6f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(5/7.0f, 0.7f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(5/7.0f, 0.8f, 0.8f));
                if (ImGui::Button(p.filename().c_str()))
                {
                    *pCurrentDirectory = std::filesystem::canonical(p);
                }
                ImGui::PopStyleColor(3);
            }
            else if (allowedExtensions.empty() || std::find(allowedExtensions.begin(), allowedExtensions.end(), p.extension()) != allowedExtensions.end())
            {
                if (ImGui::Button(p.filename().c_str()))
                {
                    *pFileToLoad = std::filesystem::canonical(p);
                    return false;
                }
            }
        }
    }
    return true;
}

void moGUIDrawEnd(VkCommandBuffer commandBuffer)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void moGUIShutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

int main(int argc, char** argv)
{
    GLFWwindow*                  window = nullptr;
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceKHR                 surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};

    // Initialization
    {
        glfwSetErrorCallback(moGlfwErrorCallback);
        glfwInit();
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

    // ImGui initialization
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = device->physicalDevice;
        init_info.Device = device->device;
        init_info.QueueFamily = device->queueFamily;
        init_info.Queue = device->queue;
        init_info.DescriptorPool = device->descriptorPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.CheckVkResultFn = device->pCheckVkResultFn;

        // Use any command queue
        VkCommandPool commandPool = swapChain->frames[0].pool;
        VkCommandBuffer commandBuffer = swapChain->frames[0].buffer;

        moGUIInit(window, device->device, swapChain->renderPass, commandPool, commandBuffer, device->queue, &init_info);
    }

    MoPipelineLayout pipelineLayout;
    moCreatePipelineLayout(&pipelineLayout);

    // Passthrough
    VkPipeline passthroughPipeline;
    moCreatePassthroughPipeline(swapChain->renderPass, pipelineLayout->pipelineLayout, &passthroughPipeline);

    MoMesh cubeMesh;
    moCreateDemoCube(&cubeMesh, float3(0.5,0.5,0.5));

    std::filesystem::path currentDirectory = std::filesystem::path(argv[0]).parent_path();
    std::filesystem::path selectedFile;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Frame begin
        MoCommandBuffer currentCommandBuffer;
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &currentCommandBuffer, &imageAcquiredSemaphore);
        moBindPipeline(currentCommandBuffer.buffer, passthroughPipeline, pipelineLayout->pipelineLayout, pipelineLayout->descriptorSet[swapChain->frameIndex]);
        moDrawMesh(currentCommandBuffer.buffer, cubeMesh);

        // ImGui
        moGUIDrawBegin();
        ImGui::Begin(std::filesystem::path(argv[0]).stem().c_str());
        ImGui::Text("DearImGui is great!");
        ImGui::Text("Selected: %s", selectedFile.c_str());
        ImGui::End();
        ImGui::Begin("Select File");
        moGUIFileBrowser(&currentDirectory, &selectedFile);
        ImGui::End();
        moGUIDrawEnd(currentCommandBuffer.buffer);

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

    // ImGui cleanup
    moGUIShutdown();

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
