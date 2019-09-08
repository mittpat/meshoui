#include "mo_material.h"
#include "mo_device.h"
#include "mo_pipeline.h"
#include "mo_swapchain.h"

#include <cstdint>
#include <vector>

using namespace linalg;
using namespace linalg::aliases;

extern MoDevice      g_Device;
extern MoSwapChain   g_SwapChain;
extern MoPipeline    g_Pipeline;
extern std::uint32_t g_FrameIndex;

void generateTexture(MoImageBuffer *pImageBuffer, const MoTextureInfo &textureInfo, const float4 &fallbackColor, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    VkFormat format = textureInfo.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_R8G8B8A8_UNORM : textureInfo.format;
    unsigned width = textureInfo.extent.width, height = textureInfo.extent.height;
    std::vector<uint8_t> data;
    const uint8_t* dataPtr = textureInfo.pData;
    if (dataPtr == nullptr)
    {
        // use fallback
        width = height = 1;
        data.resize(4);
        data[0] = (std::uint8_t)(fallbackColor.x * 0xFF);
        data[1] = (std::uint8_t)(fallbackColor.y * 0xFF);
        data[2] = (std::uint8_t)(fallbackColor.z * 0xFF);
        data[3] = (std::uint8_t)(fallbackColor.w * 0xFF);
        dataPtr = data.data();
    }
    VkDeviceSize size = textureInfo.dataSize;
    if (size == 0)
    {
        size = width * height * 4;
        switch (format)
        {
        case VK_FORMAT_R8G8B8A8_UNORM:
            break;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            size /= 6;
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
            size /= 4;
            break;
        }
    }

    VkResult err;

    // begin
    {
        err = vkResetCommandPool(g_Device->device, commandPool, 0);
        g_Device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(commandBuffer, &begin_info);
        g_Device->pCheckVkResultFn(err);
    }

    // create buffer
    moCreateBuffer(g_Device, pImageBuffer, {width, height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    // upload
    MoDeviceBuffer upload = {};
    moCreateBuffer(g_Device, &upload, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    moUploadBuffer(g_Device, upload, size, dataPtr);
    moTransferBuffer(commandBuffer, upload, *pImageBuffer, {width, height, 1});

    // end
    {
        VkSubmitInfo endInfo = {};
        endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        endInfo.commandBufferCount = 1;
        endInfo.pCommandBuffers = &commandBuffer;
        err = vkEndCommandBuffer(commandBuffer);
        g_Device->pCheckVkResultFn(err);
        err = vkQueueSubmit(g_Device->queue, 1, &endInfo, VK_NULL_HANDLE);
        g_Device->pCheckVkResultFn(err);
    }

    // wait
    err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    moDeleteBuffer(g_Device, upload);
}

void moCreateMaterial(const MoMaterialCreateInfo *pCreateInfo, MoMaterial *pMaterial)
{
    MoMaterial material = *pMaterial = new MoMaterial_T();
    *material = {};

    VkResult err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    auto & frame = g_SwapChain->frames[g_FrameIndex];
    generateTexture(&material->ambientImage,  pCreateInfo->textureAmbient,  pCreateInfo->colorAmbient,  frame.pool, frame.buffer);
    generateTexture(&material->diffuseImage,  pCreateInfo->textureDiffuse,  pCreateInfo->colorDiffuse,  frame.pool, frame.buffer);
    generateTexture(&material->normalImage,   pCreateInfo->textureNormal,   {0.f, 0.f, 0.f, 0.f},       frame.pool, frame.buffer);
    generateTexture(&material->emissiveImage, pCreateInfo->textureEmissive, pCreateInfo->colorEmissive, frame.pool, frame.buffer);
    generateTexture(&material->specularImage, pCreateInfo->textureSpecular, pCreateInfo->colorSpecular, frame.pool, frame.buffer);

    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        info.minFilter = info.magFilter = pCreateInfo->textureAmbient.filter;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &material->ambientSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureDiffuse.filter;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &material->diffuseSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureNormal.filter;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &material->normalSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureSpecular.filter;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &material->specularSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureEmissive.filter;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &material->emissiveSampler);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device->descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &g_Pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT];
        err = vkAllocateDescriptorSets(g_Device->device, &alloc_info, &material->descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorImageInfo desc_image[5] = {};
        desc_image[0].sampler = material->ambientSampler;
        desc_image[0].imageView = material->ambientImage->view;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[1].sampler = material->diffuseSampler;
        desc_image[1].imageView = material->diffuseImage->view;
        desc_image[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[2].sampler = material->normalSampler;
        desc_image[2].imageView = material->normalImage->view;
        desc_image[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[3].sampler = material->specularSampler;
        desc_image[3].imageView = material->specularImage->view;
        desc_image[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[4].sampler = material->emissiveSampler;
        desc_image[4].imageView = material->emissiveImage->view;
        desc_image[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[5] = {};
        for (uint32_t i = 0; i < 5; ++i)
        {
            write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[i].dstSet = material->descriptorSet;
            write_desc[i].dstBinding = i;
            write_desc[i].descriptorCount = 1;
            write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[i].pImageInfo = &desc_image[i];
        }
        vkUpdateDescriptorSets(g_Device->device, 5, write_desc, 0, nullptr);
    }
}

void moDestroyMaterial(MoMaterial material)
{
    vkQueueWaitIdle(g_Device->queue);
    moDeleteBuffer(g_Device, material->ambientImage);
    moDeleteBuffer(g_Device, material->diffuseImage);
    moDeleteBuffer(g_Device, material->normalImage);
    moDeleteBuffer(g_Device, material->specularImage);
    moDeleteBuffer(g_Device, material->emissiveImage);

    vkDestroySampler(g_Device->device, material->ambientSampler, nullptr);
    vkDestroySampler(g_Device->device, material->diffuseSampler, nullptr);
    vkDestroySampler(g_Device->device, material->normalSampler, nullptr);
    vkDestroySampler(g_Device->device, material->specularSampler, nullptr);
    vkDestroySampler(g_Device->device, material->emissiveSampler, nullptr);
    delete material;
}

void moBindMaterial(MoMaterial material)
{
    auto & frame = g_SwapChain->frames[g_FrameIndex];
    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_Pipeline->pipelineLayout, 1, 1, &material->descriptorSet, 0, nullptr);
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
