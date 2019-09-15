#include "mo_material.h"
#include "mo_device.h"
#include "mo_swapchain.h"

#include "mo_array.h"

#include <cstdint>
#include <vector>

using namespace linalg;
using namespace linalg::aliases;

extern MoDevice g_Device;

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
    moCreateBuffer(pImageBuffer, {width, height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    // upload
    MoDeviceBuffer upload = {};
    moCreateBuffer(&upload, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    moUploadBuffer(upload, size, dataPtr);
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

    moDeleteBuffer(upload);
}

void moCreateMaterial(const MoMaterialCreateInfo *pCreateInfo, MoMaterial *pMaterial)
{
    MoMaterial material = *pMaterial = new MoMaterial_T();
    *material = {};

    VkResult err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    generateTexture(&material->ambientImage,  pCreateInfo->textureAmbient,  pCreateInfo->colorAmbient,  pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    generateTexture(&material->diffuseImage,  pCreateInfo->textureDiffuse,  pCreateInfo->colorDiffuse,  pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    generateTexture(&material->normalImage,   pCreateInfo->textureNormal,   {0.f, 0.f, 0.f, 0.f},       pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    generateTexture(&material->emissiveImage, pCreateInfo->textureEmissive, pCreateInfo->colorEmissive, pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    generateTexture(&material->specularImage, pCreateInfo->textureSpecular, pCreateInfo->colorSpecular, pCreateInfo->commandPool, pCreateInfo->commandBuffer);

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
}

void moRegisterMaterial(MoPipelineLayout pipeline, MoMaterial material)
{
    MoMaterialRegistration registration = {};
    registration.pipelineLayout = pipeline->pipelineLayout;

    {
        VkDescriptorSetAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = g_Device->descriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT];
        VkResult err = vkAllocateDescriptorSets(g_Device->device, &info, &registration.descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorImageInfo imageDescriptor[5] = {};
        imageDescriptor[0].sampler = material->ambientSampler;
        imageDescriptor[0].imageView = material->ambientImage->view;
        imageDescriptor[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptor[1].sampler = material->diffuseSampler;
        imageDescriptor[1].imageView = material->diffuseImage->view;
        imageDescriptor[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptor[2].sampler = material->normalSampler;
        imageDescriptor[2].imageView = material->normalImage->view;
        imageDescriptor[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptor[3].sampler = material->specularSampler;
        imageDescriptor[3].imageView = material->specularImage->view;
        imageDescriptor[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptor[4].sampler = material->emissiveSampler;
        imageDescriptor[4].imageView = material->emissiveImage->view;
        imageDescriptor[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet writeDescriptor[5] = {};
        for (uint32_t i = 0; i < 5; ++i)
        {
            writeDescriptor[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptor[i].dstSet = registration.descriptorSet;
            writeDescriptor[i].dstBinding = i;
            writeDescriptor[i].descriptorCount = 1;
            writeDescriptor[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptor[i].pImageInfo = &imageDescriptor[i];
        }
        vkUpdateDescriptorSets(g_Device->device, 5, writeDescriptor, 0, VK_NULL_HANDLE);
    }

    carray_push_back(&material->pRegistrations, &material->registrationCount, registration);
}

void moDestroyMaterial(MoMaterial material)
{
    vkQueueWaitIdle(g_Device->queue);
    moDeleteBuffer(material->ambientImage);
    moDeleteBuffer(material->diffuseImage);
    moDeleteBuffer(material->normalImage);
    moDeleteBuffer(material->specularImage);
    moDeleteBuffer(material->emissiveImage);

    vkDestroySampler(g_Device->device, material->ambientSampler, VK_NULL_HANDLE);
    vkDestroySampler(g_Device->device, material->diffuseSampler, VK_NULL_HANDLE);
    vkDestroySampler(g_Device->device, material->normalSampler, VK_NULL_HANDLE);
    vkDestroySampler(g_Device->device, material->specularSampler, VK_NULL_HANDLE);
    vkDestroySampler(g_Device->device, material->emissiveSampler, VK_NULL_HANDLE);
    delete material;
}

void moBindMaterial(VkCommandBuffer commandBuffer, MoMaterial material, VkPipelineLayout pipelineLayout)
{
    for (std::uint32_t i = 0; i < material->registrationCount; ++i)
    {
        if (material->pRegistrations[i].pipelineLayout == pipelineLayout)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material->pRegistrations[i].descriptorSet, 0, VK_NULL_HANDLE);
            break;
        }
    }
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
