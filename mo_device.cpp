#include "mo_device.h"
#include "mo_buffer.h"
#include "mo_swapchain.h"

#include <cstdio>
#include <cstring>
#include <vector>

VkDebugReportCallbackEXT g_DebugReport     = VK_NULL_HANDLE;
MoDevice                 g_Device          = VK_NULL_HANDLE;
VkInstance               g_Instance        = VK_NULL_HANDLE;
MoSwapChain              g_SwapChain       = VK_NULL_HANDLE;

void moCreateInstance(MoInstanceCreateInfo *pCreateInfo, VkInstance *pInstance)
{
    VkResult err;

    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = pCreateInfo->extensionsCount;
        create_info.ppEnabledExtensionNames = pCreateInfo->pExtensions;
        if (pCreateInfo->debugReport)
        {
            // Enabling multiple validation layers grouped as LunarG standard validation
            const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = layers;

            // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
            const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (pCreateInfo->extensionsCount + 1));
            memcpy(extensions_ext, pCreateInfo->pExtensions, pCreateInfo->extensionsCount * sizeof(const char*));
            extensions_ext[pCreateInfo->extensionsCount] = "VK_EXT_debug_report";
            create_info.enabledExtensionCount = pCreateInfo->extensionsCount + 1;
            create_info.ppEnabledExtensionNames = extensions_ext;

            // Create Vulkan Instance
            err = vkCreateInstance(&create_info, VK_NULL_HANDLE, pInstance);
            pCreateInfo->pCheckVkResultFn(err);
            free(extensions_ext);

            // Get the function pointer (required for any extensions)
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(*pInstance, "vkCreateDebugReportCallbackEXT");

            // Setup the debug report callback
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
            debug_report_ci.pfnCallback = pCreateInfo->pDebugReportCallback;
            debug_report_ci.pUserData = VK_NULL_HANDLE;
            err = vkCreateDebugReportCallbackEXT(*pInstance, &debug_report_ci, VK_NULL_HANDLE, &g_DebugReport);
            pCreateInfo->pCheckVkResultFn(err);
        }
        else
        {
            err = vkCreateInstance(&create_info, VK_NULL_HANDLE, pInstance);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    g_Instance = *pInstance;
}

void moDestroyInstance(VkInstance instance)
{
    if (g_DebugReport)
    {
        auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT(instance, g_DebugReport, VK_NULL_HANDLE);
    }
    else
    {
        vkDestroyInstance(instance, VK_NULL_HANDLE);
    }

    g_Instance = VK_NULL_HANDLE;
}

void moCreateDevice(MoDeviceCreateInfo *pCreateInfo, MoDevice *pDevice)
{
    MoDevice device = *pDevice = new MoDevice_T();
    *device = {};
    device->memoryAlignment = 256;

    VkResult err;

    {
        uint32_t count;
        err = vkEnumeratePhysicalDevices(pCreateInfo->instance, &count, VK_NULL_HANDLE);
        pCreateInfo->pCheckVkResultFn(err);
        std::vector<VkPhysicalDevice> gpus(count);
        err = vkEnumeratePhysicalDevices(pCreateInfo->instance, &count, gpus.data());
        pCreateInfo->pCheckVkResultFn(err);
        device->physicalDevice = gpus[0];
    }

    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &count, VK_NULL_HANDLE);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                device->queueFamily = i;
                break;
            }
        }
    }

    {
        uint32_t device_extensions_count = 1;
        const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = device->queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.textureCompressionBC = VK_TRUE;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = countof(queue_info);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extensions_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        create_info.pEnabledFeatures = &deviceFeatures;
        err = vkCreateDevice(device->physicalDevice, &create_info, VK_NULL_HANDLE, &device->device);
        pCreateInfo->pCheckVkResultFn(err);
        vkGetDeviceQueue(device->device, device->queueFamily, 0, &device->queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * countof(pool_sizes);
        pool_info.poolSizeCount = countof(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(device->device, &pool_info, VK_NULL_HANDLE, &device->descriptorPool);
        pCreateInfo->pCheckVkResultFn(err);
    }

    if (pCreateInfo->surface != VK_NULL_HANDLE)
    {
        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, device->queueFamily, pCreateInfo->surface, &res);
        if (res != VK_TRUE)
        {
            printf("No WSI support on physical device 0\n");
        }

        pCreateInfo->pSurfaceFormat->format = VK_FORMAT_UNDEFINED;

        uint32_t avail_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, pCreateInfo->surface, &avail_count, VK_NULL_HANDLE);
        std::vector<VkSurfaceFormatKHR> avail_format(avail_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, pCreateInfo->surface, &avail_count, avail_format.data());

        if (avail_count == 1)
        {
            if (avail_format[0].format == VK_FORMAT_UNDEFINED)
            {
                pCreateInfo->pSurfaceFormat->format = pCreateInfo->pRequestFormats[0];
                pCreateInfo->pSurfaceFormat->colorSpace = pCreateInfo->requestColorSpace;
            }
            else
            {
                *pCreateInfo->pSurfaceFormat = avail_format[0];
            }
        }
        else
        {
            *pCreateInfo->pSurfaceFormat = avail_format[0];
            for (uint32_t i = 0; i < pCreateInfo->requestFormatsCount; ++i)
            {
                for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
                {
                    if (avail_format[avail_i].format == pCreateInfo->pRequestFormats[i] && avail_format[avail_i].colorSpace == pCreateInfo->requestColorSpace)
                    {
                        *pCreateInfo->pSurfaceFormat = avail_format[avail_i];
                    }
                }
            }
        }
    }

    device->pCheckVkResultFn = pCreateInfo->pCheckVkResultFn;

    g_Device = device;
}

void moDestroyDevice(MoDevice device)
{
    vkDestroyDescriptorPool(device->device, device->descriptorPool, VK_NULL_HANDLE);
    vkDestroyDevice(device->device, VK_NULL_HANDLE);
    delete device;

    g_Device = VK_NULL_HANDLE;
}

void moExternalInit(MoInitInfo *pInfo)
{
    g_Instance = pInfo->instance;
    g_Device = new MoDevice_T;
    *g_Device = {};
    g_Device->memoryAlignment = 256;
    g_Device->physicalDevice = pInfo->physicalDevice;
    g_Device->device = pInfo->device;
    g_Device->queueFamily = pInfo->queueFamily;
    g_Device->queue = pInfo->queue;
    g_Device->pCheckVkResultFn = pInfo->pCheckVkResultFn;
    g_Device->descriptorPool = pInfo->descriptorPool;
    g_SwapChain = new MoSwapChain_T;
    *g_SwapChain = {};
    g_SwapChain->depthBuffer = pInfo->depthBuffer;
    g_SwapChain->swapChainKHR = pInfo->swapChainKHR;
    g_SwapChain->renderPass = pInfo->renderPass;
    g_SwapChain->extent = pInfo->extent;
    for (uint32_t i = 0; i < pInfo->swapChainCommandBufferCount; ++i)
    {
        g_SwapChain->frames[i].pool = pInfo->pSwapChainCommandBuffers[i].pool;
        g_SwapChain->frames[i].buffer = pInfo->pSwapChainCommandBuffers[i].buffer;
        g_SwapChain->frames[i].fence = pInfo->pSwapChainCommandBuffers[i].fence;
        g_SwapChain->frames[i].acquired = pInfo->pSwapChainCommandBuffers[i].acquired;
        g_SwapChain->frames[i].complete = pInfo->pSwapChainCommandBuffers[i].complete;
    }
    for (uint32_t i = 0; i < pInfo->swapChainSwapBufferCount; ++i)
    {
        g_SwapChain->images[i].back = pInfo->pSwapChainSwapBuffers[i].back;
        g_SwapChain->images[i].view = pInfo->pSwapChainSwapBuffers[i].view;
        g_SwapChain->images[i].front = pInfo->pSwapChainSwapBuffers[i].front;
        g_SwapChain->images[i].memory = pInfo->pSwapChainSwapBuffers[i].memory;
    }
}

void moExternalShutdown()
{
    g_Instance = VK_NULL_HANDLE;
    g_Device->physicalDevice = VK_NULL_HANDLE;
    g_Device->device = VK_NULL_HANDLE;
    g_Device->queueFamily = -1;
    g_Device->queue = VK_NULL_HANDLE;
    g_Device->memoryAlignment = 256;
    g_SwapChain->swapChainKHR = VK_NULL_HANDLE;
    g_SwapChain->renderPass = VK_NULL_HANDLE;
    g_SwapChain->extent = {0, 0};
    g_Device->descriptorPool = VK_NULL_HANDLE;
    delete g_Device;
    delete g_SwapChain;
    g_Device = VK_NULL_HANDLE;
    g_SwapChain = VK_NULL_HANDLE;
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
