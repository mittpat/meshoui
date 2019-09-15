#pragma once

#include <vulkan/vulkan.h>

typedef struct MoInstanceCreateInfo {
    const char* const*           pExtensions;
    uint32_t                     extensionsCount;
    VkBool32                     debugReport;
    PFN_vkDebugReportCallbackEXT pDebugReportCallback;
    void                       (*pCheckVkResultFn)(VkResult err);
} MoInstanceCreateInfo;

typedef struct MoDeviceCreateInfo {
    VkInstance          instance;
    VkSurfaceKHR        surface;
    const VkFormat*     pRequestFormats;
    uint32_t            requestFormatsCount;
    VkColorSpaceKHR     requestColorSpace;
    VkSurfaceFormatKHR* pSurfaceFormat;
    void              (*pCheckVkResultFn)(VkResult err);
} MoDeviceCreateInfo;

typedef struct MoDevice_T {
    VkPhysicalDevice physicalDevice;
    VkDevice         device;
    uint32_t         queueFamily;
    VkQueue          queue;
    VkDescriptorPool descriptorPool;
    VkDeviceSize     memoryAlignment;
    void           (*pCheckVkResultFn)(VkResult err);
}* MoDevice;

typedef struct MoSwapBuffer MoSwapBuffer;
typedef struct MoCommandBuffer MoCommandBuffer;
typedef struct MoImageBuffer_T* MoImageBuffer;

// you must call moInit(MoInitInfo) before creating a mesh or material, typically when starting your application
// call moShutdown() when closing your application
// initialization uses only Vulkan API objects, it is therefore easy to integrate without calling moCreateInstance,
// moCreateDevice and moCreateSwapChain
typedef struct MoInitInfo {
    VkInstance                   instance;
    VkPhysicalDevice             physicalDevice;
    VkDevice                     device;
    uint32_t                     queueFamily;
    VkQueue                      queue;
    VkDescriptorPool             descriptorPool;
    const MoSwapBuffer*          pSwapChainSwapBuffers;
    uint32_t                     swapChainSwapBufferCount;
    const MoCommandBuffer*       pSwapChainCommandBuffers;
    uint32_t                     swapChainCommandBufferCount;
    MoImageBuffer                depthBuffer;
    VkSwapchainKHR               swapChainKHR;
    VkRenderPass                 renderPass;
    VkExtent2D                   extent;
    void                         (*pCheckVkResultFn)(VkResult err);
} MoInitInfo;

// you can create a VkInstance using moCreateInstance(MoInstanceCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing instance
void moCreateInstance(MoInstanceCreateInfo* pCreateInfo, VkInstance* pInstance);

// free instance
void moDestroyInstance(VkInstance instance);

// you can create a VkDevice using moCreateDevice(MoDeviceCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing device
void moCreateDevice(MoDeviceCreateInfo* pCreateInfo, MoDevice* pDevice);

// free device
void moDestroyDevice(MoDevice device);

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
