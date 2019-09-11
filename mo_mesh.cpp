#include "mo_mesh.h"

#include "mo_device.h"
#include "mo_swapchain.h"

#include <cstdint>

using namespace linalg;
using namespace linalg::aliases;

extern MoDevice      g_Device;
extern MoSwapChain   g_SwapChain;
extern std::uint32_t g_FrameIndex;

void moCreateMesh(const MoMeshCreateInfo *pCreateInfo, MoMesh *pMesh)
{
    MoMesh mesh = *pMesh = new MoMesh_T();
    *mesh = {};

    mesh->indexBufferSize = pCreateInfo->indexCount;
    mesh->vertexCount = pCreateInfo->vertexCount;
    const VkDeviceSize index_size = pCreateInfo->indexCount * sizeof(uint32_t);
    moCreateBuffer(g_Device, &mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(g_Device, &mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(g_Device, &mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(g_Device, &mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(g_Device, &mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    moCreateBuffer(g_Device, &mesh->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    moUploadBuffer(g_Device, mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pVertices);
    moUploadBuffer(g_Device, mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), pCreateInfo->pTextureCoords);
    moUploadBuffer(g_Device, mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pNormals);
    moUploadBuffer(g_Device, mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pTangents);
    moUploadBuffer(g_Device, mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pBitangents);
    moUploadBuffer(g_Device, mesh->indexBuffer, index_size, pCreateInfo->pIndices);
}

void moDestroyMesh(MoMesh mesh)
{
    vkQueueWaitIdle(g_Device->queue);
    moDeleteBuffer(g_Device, mesh->verticesBuffer);
    moDeleteBuffer(g_Device, mesh->textureCoordsBuffer);
    moDeleteBuffer(g_Device, mesh->normalsBuffer);
    moDeleteBuffer(g_Device, mesh->tangentsBuffer);
    moDeleteBuffer(g_Device, mesh->bitangentsBuffer);
    moDeleteBuffer(g_Device, mesh->indexBuffer);
    delete mesh;
}

void moDrawMesh(MoMesh mesh)
{
    auto & frame = g_SwapChain->frames[g_FrameIndex];

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
    vkCmdBindVertexBuffers(frame.buffer, 0, 5, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(frame.buffer, mesh->indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(frame.buffer, mesh->indexBufferSize, 1, 0, 0, 0);
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