#include "mo_mesh.h"

#include "mo_array.h"
#include "mo_bvh.h"
#include "mo_device.h"
#include "mo_swapchain.h"

#include <cstdint>

using namespace linalg;
using namespace linalg::aliases;

extern MoDevice g_Device;

void moCreateMesh(const MoMeshCreateInfo *pCreateInfo, MoMesh *pMesh)
{
    MoMesh mesh = *pMesh = new MoMesh_T();
    *mesh = {};

    mesh->indexBufferSize = pCreateInfo->indexCount;
    mesh->vertexCount = pCreateInfo->vertexCount;
    const VkDeviceSize index_size = pCreateInfo->indexCount * sizeof(uint32_t);
    moCreateBuffer(&mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(&mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(&mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(&mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(&mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(&mesh->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    moUploadBuffer(mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pVertices);
    moUploadBuffer(mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), pCreateInfo->pTextureCoords);
    moUploadBuffer(mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pNormals);
    moUploadBuffer(mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pTangents);
    moUploadBuffer(mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pBitangents);
    moUploadBuffer(mesh->indexBuffer, index_size, pCreateInfo->pIndices);

    if (pCreateInfo->bvh && pCreateInfo->bvh->splitNodeCount)
    {
        VkDeviceSize objectsSize = sizeof(MoTriangle) * pCreateInfo->bvh->triangleCount;
        moCreateBuffer(&mesh->bvhObjectBuffer, objectsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        moUploadBuffer(mesh->bvhObjectBuffer, objectsSize, pCreateInfo->bvh->pTriangles);

        VkDeviceSize nodesSize = sizeof(MoBVHSplitNode) * pCreateInfo->bvh->splitNodeCount;
        moCreateBuffer(&mesh->bvhNodesBuffer, nodesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        moUploadBuffer(mesh->bvhNodesBuffer, nodesSize, pCreateInfo->bvh->pSplitNodes);
    }
    else
    {
        uint32_t data[4] = {};
        VkDeviceSize size = sizeof(data);

        moCreateBuffer(&mesh->bvhObjectBuffer, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        moUploadBuffer(mesh->bvhObjectBuffer, size, &data);

        moCreateBuffer(&mesh->bvhNodesBuffer, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        moUploadBuffer(mesh->bvhNodesBuffer, size, &data);
    }
}

void moRegisterMesh(MoPipelineLayout pipeline, MoMesh mesh)
{
    MoMeshRegistration registration = {};
    registration.pipelineLayout = pipeline->pipelineLayout;

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device->descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &pipeline->descriptorSetLayout[MO_SSBO_DESC_LAYOUT];
        VkResult err = vkAllocateDescriptorSets(g_Device->device, &alloc_info, &registration.descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    VkDescriptorBufferInfo desc_buffer[2] = {};
    desc_buffer[0].buffer = mesh->bvhObjectBuffer->buffer;
    desc_buffer[0].offset = 0;
    desc_buffer[0].range = mesh->bvhObjectBuffer->size;
    desc_buffer[1].buffer = mesh->bvhNodesBuffer->buffer;
    desc_buffer[1].offset = 0;
    desc_buffer[1].range = mesh->bvhNodesBuffer->size;

    VkWriteDescriptorSet write_desc[2] = {};
    for (uint32_t i = 0; i < 2; ++i)
    {
        write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[i].dstSet = registration.descriptorSet;
        write_desc[i].dstBinding = i;
        write_desc[i].dstArrayElement = 0;
        write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_desc[i].descriptorCount = 1;
        write_desc[i].pBufferInfo = &desc_buffer[i];
    }
    vkUpdateDescriptorSets(g_Device->device, 2, write_desc, 0, nullptr);

    carray_push_back(&mesh->pRegistrations, &mesh->registrationCount, registration);
}

void moDestroyMesh(MoMesh mesh)
{
    vkQueueWaitIdle(g_Device->queue);
    moDeleteBuffer(mesh->verticesBuffer);
    moDeleteBuffer(mesh->textureCoordsBuffer);
    moDeleteBuffer(mesh->normalsBuffer);
    moDeleteBuffer(mesh->tangentsBuffer);
    moDeleteBuffer(mesh->bitangentsBuffer);
    moDeleteBuffer(mesh->indexBuffer);
    moDeleteBuffer(mesh->bvhObjectBuffer);
    moDeleteBuffer(mesh->bvhNodesBuffer);
    for (std::uint32_t i = 0; i < mesh->registrationCount; ++i)
    {
        vkFreeDescriptorSets(g_Device->device, g_Device->descriptorPool, 1, &mesh->pRegistrations[i].descriptorSet);
    }
    carray_free(mesh->pRegistrations, &mesh->registrationCount);
    delete mesh;
}

void moBindMesh(VkCommandBuffer commandBuffer, MoMesh mesh, VkPipelineLayout pipelineLayout)
{
    for (std::uint32_t i = 0; i < mesh->registrationCount; ++i)
    {
        if (mesh->pRegistrations[i].pipelineLayout == pipelineLayout)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, MO_SSBO_DESC_LAYOUT, 1, &mesh->pRegistrations[i].descriptorSet, 0, VK_NULL_HANDLE);
            break;
        }
    }
}

void moDrawMesh(VkCommandBuffer commandBuffer, MoMesh mesh)
{
    VkBuffer vertexBuffers[] = {mesh->verticesBuffer->buffer,
                                mesh->textureCoordsBuffer->buffer,
                                mesh->normalsBuffer->buffer,
                                mesh->tangentsBuffer->buffer,
                                mesh->bitangentsBuffer->buffer};
    VkDeviceSize offsets[] = {0,
                              0,
                              0,
                              0,
                              0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 5, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, mesh->indexBufferSize, 1, 0, 0, 0);
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
