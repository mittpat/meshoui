#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
#include "mo_device.h"
#include "mo_swapchain.h"
#include "mo_buffer.h"
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <signal.h>

using namespace std::filesystem;

static const struct { const char* label; const char* binary; } g_examples[] = {
    { "cube_passthrough",    "cube_passthrough"    },
    { "cube_imgui",          "cube_imgui"          },
    { "teapot_viewer",       "teapot_viewer"       },
    { "teapot_blur",         "teapot_blur"         },
    { "teapot_unwrap",       "teapot_unwrap"       },
    { "teapot_uvrenderpass", "teapot_uvrenderpass" },
    { "teapot_readback",     "teapot_readback"     },
    { "material_viewer",     "material_viewer"     },
    { "cave_walking",        "cave_walking"        },
};
static const int g_exampleCount = (int)(sizeof(g_examples)/sizeof(g_examples[0]));

static const int THUMB_W = 128, THUMB_H = 128;

struct MoThumbTexture {
    VkImage         image;
    VkDeviceMemory  memory;
    VkImageView     view;
    VkSampler       sampler;
    VkDescriptorSet set;
};

static MoThumbTexture g_thumbs[9];

extern MoDevice g_Device;

static void moMakeThumbnail(int index, uint8_t* out)
{
    float h = (float)index / g_exampleCount, s = 0.7f, v = 0.85f;
    int   i = (int)(h * 6.f);
    float f = h * 6.f - i, p = v*(1-s), q = v*(1-s*f), t = v*(1-s*(1-f));
    float r, g, b;
    switch (i % 6) {
        case 0: r=v;g=t;b=p; break; case 1: r=q;g=v;b=p; break;
        case 2: r=p;g=v;b=t; break; case 3: r=p;g=q;b=v; break;
        case 4: r=t;g=p;b=v; break; default:r=v;g=p;b=q; break;
    }
    uint8_t R=r*255, G=g*255, B=b*255;
    for (int y=0; y<THUMB_H; ++y)
        for (int x=0; x<THUMB_W; ++x) {
            bool border = (x<8||x>=THUMB_W-8||y<8||y>=THUMB_H-8);
            uint8_t* px = out + (y*THUMB_W+x)*4;
            px[0]=border?R/2:R; px[1]=border?G/2:G;
            px[2]=border?B/2:B; px[3]=255;
        }
}

void moCreateThumbDescriptorLayout(VkDevice dev, VkDescriptorSetLayout* layout)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings    = &binding;
    vkCreateDescriptorSetLayout(dev, &info, VK_NULL_HANDLE, layout);
}

void moUploadThumbTexture(VkDevice dev, VkPhysicalDevice gpu,
                          VkCommandPool pool, VkQueue queue,
                          VkDescriptorPool descPool,
                          VkDescriptorSetLayout descLayout,
                          const uint8_t* pixels, MoThumbTexture* t)
{
    VkResult err;

    // Staging buffer
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = THUMB_W * THUMB_H * 4;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer stagingBuffer;
    err = vkCreateBuffer(dev, &stagingInfo, VK_NULL_HANDLE, &stagingBuffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, stagingBuffer, &req);
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = moMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, req.memoryTypeBits);
    VkDeviceMemory stagingMemory;
    err = vkAllocateMemory(dev, &allocInfo, VK_NULL_HANDLE, &stagingMemory);
    err = vkBindBufferMemory(dev, stagingBuffer, stagingMemory, 0);

    void* data;
    err = vkMapMemory(dev, stagingMemory, 0, stagingInfo.size, 0, &data);
    memcpy(data, pixels, stagingInfo.size);
    vkUnmapMemory(dev, stagingMemory);

    // VkImage
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {THUMB_W, THUMB_H, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    err = vkCreateImage(dev, &imgInfo, VK_NULL_HANDLE, &t->image);

    vkGetImageMemoryRequirements(dev, t->image, &req);
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = moMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(dev, &allocInfo, VK_NULL_HANDLE, &t->memory);
    err = vkBindImageMemory(dev, t->image, t->memory, 0);

    // Command buffer for transfer
    VkCommandBufferAllocateInfo cmdInfo = {};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandPool = pool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    VkCommandBuffer cmdBuf;
    err = vkAllocateCommandBuffers(dev, &cmdInfo, &cmdBuf);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Barrier: UNDEFINED -> TRANSFER_DST
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = t->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

    // Copy
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowPitch = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {THUMB_W, THUMB_H, 1};
    vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Barrier: TRANSFER_DST -> SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

    err = vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    err = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    err = vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(dev, pool, 1, &cmdBuf);
    vkDestroyBuffer(dev, stagingBuffer, VK_NULL_HANDLE);
    vkFreeMemory(dev, stagingMemory, VK_NULL_HANDLE);

    // ImageView
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = t->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    err = vkCreateImageView(dev, &viewInfo, VK_NULL_HANDLE, &t->view);

    // Sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    err = vkCreateSampler(dev, &samplerInfo, VK_NULL_HANDLE, &t->sampler);

    // Descriptor set
    VkDescriptorSetAllocateInfo descAllocInfo = {};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = descPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &descLayout;
    err = vkAllocateDescriptorSets(dev, &descAllocInfo, &t->set);

    VkDescriptorImageInfo descImageInfo = {};
    descImageInfo.sampler = t->sampler;
    descImageInfo.imageView = t->view;
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = t->set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &descImageInfo;
    vkUpdateDescriptorSets(dev, 1, &write, 0, VK_NULL_HANDLE);
}

void moDestroyThumbTexture(VkDevice dev, MoThumbTexture* t)
{
    vkDestroySampler(dev, t->sampler, VK_NULL_HANDLE);
    vkDestroyImageView(dev, t->view, VK_NULL_HANDLE);
    vkFreeMemory(dev, t->memory, VK_NULL_HANDLE);
    vkDestroyImage(dev, t->image, VK_NULL_HANDLE);
}

static void moLaunchExample(const char* dir, const char* name)
{
#ifdef _WIN32
    char cmd[512]; snprintf(cmd,512,"%s\\%s.exe",dir,name);
    STARTUPINFOA si={};si.cb=sizeof(si);
    PROCESS_INFORMATION pi={};
    CreateProcessA(cmd,0,0,0,FALSE,0,0,0,&si,&pi);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
#else
    char path[512]; snprintf(path,512,"%s/%s",dir,name);
    pid_t pid = fork();
    if (pid == 0) { execl(path,name,(char*)nullptr); _exit(1); }
#endif
}

void moGUIInit(MoSwapChain swapChain, GLFWwindow* window, VkDescriptorPool descriptorPool)
{
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Device->instance;
    init_info.PhysicalDevice = g_Device->physicalDevice;
    init_info.Device = g_Device->device;
    init_info.QueueFamily = g_Device->queueFamily;
    init_info.Queue = g_Device->queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    init_info.MinImageCount = MO_FRAME_COUNT;
    init_info.ImageCount = swapChain->imageCount > MO_FRAME_COUNT ? swapChain->imageCount : MO_FRAME_COUNT;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info, swapChain->renderPass);

    // Upload font
    {
        VkCommandPool pool;
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = g_Device->queueFamily;
        vkCreateCommandPool(g_Device->device, &poolInfo, VK_NULL_HANDLE, &pool);

        VkCommandBuffer cmdBuf;
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_Device->device, &allocInfo, &cmdBuf);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &beginInfo);
        ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
        vkEndCommandBuffer(cmdBuf);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;
        vkQueueSubmit(g_Device->queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_Device->queue);

        vkFreeCommandBuffers(g_Device->device, pool, 1, &cmdBuf);
        vkDestroyCommandPool(g_Device->device, pool, VK_NULL_HANDLE);
    }
}

void moGUIDrawBegin()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void moGUIDrawEnd(VkCommandBuffer cb)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
}

void moGUIShutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

int main(int argc, char** argv)
{
    signal(SIGCHLD, SIG_IGN);

    std::string binaryDir = filesystem::path(argv[0]).parent_path().string();

    glfwSetErrorCallback([](int, const char* err) { printf("GLFW Error: %s\n", err); });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(960, 540, "meshoui launcher", NULL, NULL);
    if (!window) return 1;

    VkResult err;
    MoDevice device = nullptr;
    {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "launcher";
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        moCreateInstance(&createInfo, &device);
    }

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(device->instance, window, VK_NULL_HANDLE, &surface);

    moCreateDevice(device);

    MoSwapChain swapChain = nullptr;
    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        VkSurfaceFormatKHR surfaceFormat = {};
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        MoSwapChainCreateInfo createInfo = {};
        createInfo.surface = surface;
        createInfo.surfaceFormat = surfaceFormat;
        createInfo.extent = {(uint32_t)w, (uint32_t)h};
        createInfo.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        createInfo.vsync = true;
        createInfo.offscreen = false;
        createInfo.pCheckVkResultFn = [](VkResult r) { if(r!=VK_SUCCESS) printf("VkError: %d\n",r); };
        moCreateSwapChain(&createInfo, &swapChain);
    }

    VkDescriptorPool descPool;
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 64;
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 64;
        vkCreateDescriptorPool(device->device, &poolInfo, VK_NULL_HANDLE, &descPool);
    }

    moGUIInit(swapChain, window, descPool);

    VkDescriptorSetLayout thumbLayout;
    moCreateThumbDescriptorLayout(device->device, &thumbLayout);

    uint8_t pixels[9][THUMB_W*THUMB_H*4];
    for (int i = 0; i < g_exampleCount; ++i) {
        moMakeThumbnail(i, pixels[i]);
        moUploadThumbTexture(device->device, device->physicalDevice,
                           swapChain->frames[0].pool, device->queue,
                           descPool, thumbLayout, pixels[i], &g_thumbs[i]);
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        VkSemaphore acquiredSemaphore;
        MoCommandBuffer currentCommandBuffer;
        moBeginSwapChain(swapChain, &currentCommandBuffer, &acquiredSemaphore);

        moBeginRenderPass(swapChain, currentCommandBuffer);
        moGUIDrawBegin();

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Examples", nullptr,
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);

        const float SIZE=200, PAD=16; const int COLS=3;
        for (int i=0; i<g_exampleCount; ++i) {
            if (i>0 && (i%COLS)!=0) ImGui::SameLine(0,PAD);
            ImGui::BeginGroup();
            ImGui::PushID(i);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            if (ImGui::ImageButton((ImTextureID)(intptr_t)g_thumbs[i].set, {SIZE,SIZE}))
                moLaunchExample(binaryDir.c_str(), g_examples[i].binary);
            ImGui::PopStyleVar();
            ImGui::PopID();
            float tw = ImGui::CalcTextSize(g_examples[i].label).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (SIZE-tw)*0.5f);
            ImGui::TextUnformatted(g_examples[i].label);
            ImGui::EndGroup();
        }
        ImGui::End();

        moGUIDrawEnd(currentCommandBuffer.buffer);
        vkCmdEndRenderPass(currentCommandBuffer.buffer);

        err = moEndSwapChain(swapChain, &acquiredSemaphore);
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            int w, h;
            glfwGetWindowSize(window, &w, &h);
            MoSwapChainRecreateInfo recreateInfo = {};
            recreateInfo.surface = surface;
            recreateInfo.surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            recreateInfo.extent = {(uint32_t)w, (uint32_t)h};
            recreateInfo.vsync = true;
            recreateInfo.offscreen = false;
            moRecreateSwapChain(&recreateInfo, swapChain);
        }
    }

    vkDeviceWaitIdle(device->device);

    for (int i = 0; i < g_exampleCount; ++i)
        moDestroyThumbTexture(device->device, &g_thumbs[i]);

    vkDestroyDescriptorSetLayout(device->device, thumbLayout, VK_NULL_HANDLE);
    moGUIShutdown();
    vkDestroyDescriptorPool(device->device, descPool, VK_NULL_HANDLE);
    moDestroySwapChain(swapChain);
    vkDestroySurfaceKHR(device->instance, surface, VK_NULL_HANDLE);
    moDestroyDevice(device);
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
