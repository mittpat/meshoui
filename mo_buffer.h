#pragma once

#include "mo_device.h"

#include <vulkan/vulkan.h>

#include <linalg.h>

typedef struct MoDeviceBuffer_T {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
}* MoDeviceBuffer;

typedef struct MoImageBuffer_T {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
}* MoImageBuffer;

typedef struct MoSwapBuffer {
    VkImage        back;
    VkImageView    view;
    VkFramebuffer  front;
    VkDeviceMemory memory;
} MoSwapBuffer;

typedef struct MoCommandBuffer {
    VkCommandPool   pool;
    VkCommandBuffer buffer;
    VkFence         fence;
    VkSemaphore     acquired;
    VkSemaphore     complete;
} MoCommandBuffer;

typedef struct MoPushConstant {
    linalg::aliases::float4x4 model;
    linalg::aliases::float4x4 view;
    linalg::aliases::float4x4 projection;
} MoPushConstant;

typedef struct MoUniform {
    alignas(16) linalg::aliases::float3 camera;
    alignas(16) linalg::aliases::float3 light;
} MoUniform;

uint32_t moMemoryType(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits);

void moCreateBuffer(MoDevice device, MoDeviceBuffer *pDeviceBuffer, VkDeviceSize size, VkBufferUsageFlags usage);

void moUploadBuffer(MoDevice device, MoDeviceBuffer deviceBuffer, VkDeviceSize dataSize, const void *pData);

void moDeleteBuffer(MoDevice device, MoDeviceBuffer deviceBuffer);

void moCreateBuffer(MoDevice device, MoImageBuffer *pImageBuffer, const VkExtent3D & extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask);

void moTransferBuffer(VkCommandBuffer commandBuffer, MoDeviceBuffer fromBuffer, MoImageBuffer toBuffer, const VkExtent3D & extent);

void moDeleteBuffer(MoDevice device, MoImageBuffer imageBuffer);

// set view's projection and view matrices, and the mesh's model matrix (as a push constant)
void moSetPMV(const MoPushConstant* pProjectionModelView);

// set the camera's position and light position (as a UBO)
void moSetLight(const MoUniform* pLightAndCamera);

template <typename T, size_t N> std::uint32_t countof(T (& arr)[N]) { return std::uint32_t(std::extent<T[N]>::value); }

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