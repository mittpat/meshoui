#include "mo_buffer.h"
#include "mo_pipeline.h"
#include "mo_swapchain.h"

#include <cstring>

extern MoDevice g_Device;

uint32_t moMemoryType(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF;
}

void moCreateBuffer(MoDevice device, MoDeviceBuffer *pDeviceBuffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
    MoDeviceBuffer deviceBuffer = *pDeviceBuffer = new MoDeviceBuffer_T();
    *deviceBuffer = {};

    VkResult err;
    {
        VkDeviceSize buffer_size_aligned = ((size - 1) / device->memoryAlignment + 1) * device->memoryAlignment;
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buffer_size_aligned;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(device->device, &buffer_info, VK_NULL_HANDLE, &deviceBuffer->buffer);
        device->pCheckVkResultFn(err);
    }
    {
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device->device, deviceBuffer->buffer, &req);
        device->memoryAlignment = (device->memoryAlignment > req.alignment) ? device->memoryAlignment : req.alignment;
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = moMemoryType(device->physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device->device, &alloc_info, VK_NULL_HANDLE, &deviceBuffer->memory);
        device->pCheckVkResultFn(err);
    }

    err = vkBindBufferMemory(device->device, deviceBuffer->buffer, deviceBuffer->memory, 0);
    device->pCheckVkResultFn(err);
    deviceBuffer->size = size;
}

void moUploadBuffer(MoDevice device, MoDeviceBuffer deviceBuffer, VkDeviceSize dataSize, const void *pData)
{
    VkResult err;
    {
        void* dest = nullptr;
        err = vkMapMemory(device->device, deviceBuffer->memory, 0, dataSize, 0, &dest);
        device->pCheckVkResultFn(err);
        memcpy(dest, pData, dataSize);
    }
    {
        VkMappedMemoryRange range = {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = deviceBuffer->memory;
        range.size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(device->device, 1, &range);
        device->pCheckVkResultFn(err);
    }

    vkUnmapMemory(device->device, deviceBuffer->memory);
}

void moDeleteBuffer(MoDevice device, MoDeviceBuffer deviceBuffer)
{
    vkDestroyBuffer(device->device, deviceBuffer->buffer, VK_NULL_HANDLE);
    vkFreeMemory(device->device, deviceBuffer->memory, VK_NULL_HANDLE);
    delete deviceBuffer;
}

void moCreateBuffer(MoDevice device, MoImageBuffer *pImageBuffer, const VkExtent3D & extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask)
{
    MoImageBuffer imageBuffer = *pImageBuffer = new MoImageBuffer_T();
    *imageBuffer = {};

    VkResult err;
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = extent;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(device->device, &info, VK_NULL_HANDLE, &imageBuffer->image);
        device->pCheckVkResultFn(err);
    }
    {
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device->device, imageBuffer->image, &req);
        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = req.size;
        info.memoryTypeIndex = moMemoryType(device->physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device->device, &info, VK_NULL_HANDLE, &imageBuffer->memory);
        device->pCheckVkResultFn(err);
        err = vkBindImageMemory(device->device, imageBuffer->image, imageBuffer->memory, 0);
        device->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = imageBuffer->image;
        info.format = format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        info.subresourceRange.aspectMask = aspectMask;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        err = vkCreateImageView(device->device, &info, VK_NULL_HANDLE, &imageBuffer->view);
        device->pCheckVkResultFn(err);
    }
}

void moTransferBuffer(VkCommandBuffer commandBuffer, MoDeviceBuffer fromBuffer, MoImageBuffer toBuffer, const VkExtent3D & extent)
{
    {
        VkImageMemoryBarrier copy_barrier = {};
        copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.image = toBuffer->image;
        copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier.subresourceRange.levelCount = 1;
        copy_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &copy_barrier);
    }
    {
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(commandBuffer, fromBuffer->buffer, toBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
    {
        VkImageMemoryBarrier use_barrier = {};
        use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.image = toBuffer->image;
        use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier.subresourceRange.levelCount = 1;
        use_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &use_barrier);
    }
}

void moDeleteBuffer(MoDevice device, MoImageBuffer imageBuffer)
{
    vkDestroyImageView(device->device, imageBuffer->view, VK_NULL_HANDLE);
    vkDestroyImage(device->device, imageBuffer->image, VK_NULL_HANDLE);
    vkFreeMemory(device->device, imageBuffer->memory, VK_NULL_HANDLE);
    delete imageBuffer;
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
