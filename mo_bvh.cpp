#include "mo_bvh.h"
#include "mo_array.h"

#include <assimp/mesh.h>

#include <cstdint>

using namespace linalg;
using namespace linalg::aliases;

MoBBox::MoBBox(const float3& _min, const float3& _max)
    : min(_min)
    , max(_max)
{
}

MoBBox::MoBBox(const float3& point)
    : min(point)
    , max(point)
{
}

std::uint32_t MoBBox::longestSide() const
{
    float3 extent = max - min;

    std::uint32_t dimension = 0;
    if (extent.y > extent.x)
    {
        dimension = 1;
        if (extent.z > extent.y)
        {
            dimension = 2;
        }
    }
    else if (extent.z > extent.x)
    {
        dimension = 2;
    }
    return dimension;
}

void MoBBox::expandToInclude(const float3& point)
{
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
}

void MoBBox::expandToInclude(const MoBBox& box)
{
    min.x = std::min(min.x, box.min.x);
    min.y = std::min(min.y, box.min.y);
    min.z = std::min(min.z, box.min.z);
    max.x = std::max(max.x, box.max.x);
    max.y = std::max(max.y, box.max.y);
    max.z = std::max(max.z, box.max.z);
}

void moCreateBVH(const aiMesh * ai_mesh, MoBVH *pBVH)
{
    MoBVH bvh = *pBVH = new MoBVH_T();
    *bvh = {};

    carray_resize(&bvh->pTriangles, &bvh->triangleCount, ai_mesh->mNumFaces);
    for (std::uint32_t faceIdx = 0; faceIdx < ai_mesh->mNumFaces; ++faceIdx)
    {
        const auto* face = &ai_mesh->mFaces[faceIdx];
        switch (face->mNumIndices)
        {
        case 3:
        {
            MoTriangle& triangle = const_cast<MoTriangle&>(bvh->pTriangles[faceIdx]);
            triangle.v0 = float3((float*)&ai_mesh->mVertices[face->mIndices[0]]);
            triangle.v1 = float3((float*)&ai_mesh->mVertices[face->mIndices[1]]);
            triangle.v2 = float3((float*)&ai_mesh->mVertices[face->mIndices[2]]);
            if (ai_mesh->HasTextureCoords(0))
            {
                triangle.uv0 = float2((float*)&ai_mesh->mTextureCoords[0][face->mIndices[0]]);
                triangle.uv1 = float2((float*)&ai_mesh->mTextureCoords[0][face->mIndices[1]]);
                triangle.uv2 = float2((float*)&ai_mesh->mTextureCoords[0][face->mIndices[2]]);
            }
            triangle.n0 = float3((float*)&ai_mesh->mNormals[face->mIndices[0]]);
            triangle.n1 = float3((float*)&ai_mesh->mNormals[face->mIndices[1]]);
            triangle.n2 = float3((float*)&ai_mesh->mNormals[face->mIndices[2]]);
            break;
        }
        default:
            printf("parseAiMesh(): Mesh %s, Face %d has %d vertices.\n", ai_mesh->mName.C_Str(), faceIdx, face->mNumIndices);
            break;
        }
    }

    enum : std::uint32_t
    {
        Node_Untouched    = 0xffffffff,
        Node_TouchedTwice = 0xfffffffd,
        Node_Root         = 0xfffffffc,
    };

    std::uint32_t currentSize = 0;
    const std::uint32_t* indices = nullptr;
    carray_resize(&indices, &currentSize, bvh->triangleCount);
    for (std::uint32_t i = 0; i < currentSize; ++i)
        const_cast<std::uint32_t*>(indices)[i] = i;

    bvh->splitNodeCount = 0;
    std::uint32_t stackPtr = 0;

    struct Entry
    {
        std::uint32_t parent;
        std::uint32_t start;
        std::uint32_t end;
    };
    Entry* entries = {};
    std::uint32_t entryCount = 0;
    carray_resize(&entries, &entryCount, 1);
    entries[stackPtr].start = 0;
    entries[stackPtr].end = currentSize;
    entries[stackPtr].parent = Node_Root;
    stackPtr++;

    std::uint32_t splitNodeCount = 0;

    MoBVHSplitNode splitNode;
    while (stackPtr > 0)
    {
        Entry &entry = entries[--stackPtr];
        std::uint32_t start = entry.start;
        std::uint32_t end = entry.end;
        std::uint32_t count = end - start;

        splitNodeCount++;
        splitNode.start = start;
        splitNode.offset = Node_Untouched;

        MoBBox boundingBox = bvh->pTriangles[indices[start]].getBoundingBox();
        MoBBox boundingBoxCentroids = bvh->pTriangles[indices[start]].getCentroid();
        for (std::uint32_t i = start + 1; i < end; ++i)
        {
            boundingBox.expandToInclude(bvh->pTriangles[indices[i]].getBoundingBox());
            boundingBoxCentroids.expandToInclude(bvh->pTriangles[indices[i]].getCentroid());
        }
        splitNode.boundingBox = boundingBox;

        // we're at the leaf
        if (count <= 1)
        {
            splitNode.offset = 0;
        }

        carray_push_back(&bvh->pSplitNodes, &bvh->splitNodeCount, splitNode);
        if (entry.parent != Node_Root)
        {
            MoBVHSplitNode & splitNode = const_cast<MoBVHSplitNode*>(bvh->pSplitNodes)[entry.parent];
            splitNode.offset--;
            if (splitNode.offset == Node_TouchedTwice)
            {
                splitNode.offset = splitNodeCount - 1 - entry.parent;
            }
        }

        if (splitNode.offset == 0)
        {
            continue;
        }

        // find longest bbox side
        std::uint32_t splitDimension = boundingBoxCentroids.longestSide();
        float splitLength = .5f * (boundingBoxCentroids.min[splitDimension] + boundingBoxCentroids.max[splitDimension]);

        std::uint32_t mid = start;
        for (std::uint32_t i = start; i < end; ++i)
        {
            if (bvh->pTriangles[indices[i]].getCentroid()[splitDimension] < splitLength)
            {
                std::uint32_t* lObjects = const_cast<std::uint32_t*>(indices);
                std::swap(lObjects[i], lObjects[mid]);
                ++mid;
            }
        }

        if (mid == start || mid == end)
        {
            mid = start + (end-start) / 2;
        }

        // left
        if (entryCount <= stackPtr)
            carray_push_back(&entries, &entryCount, {});
        entries[stackPtr].start = mid;
        entries[stackPtr].end = end;
        entries[stackPtr].parent = splitNodeCount - 1;
        stackPtr++;

        // right
        if (entryCount <= stackPtr)
            carray_push_back(&entries, &entryCount, {});
        entries[stackPtr].start = start;
        entries[stackPtr].end = mid;
        entries[stackPtr].parent = splitNodeCount - 1;
        stackPtr++;
    }

    for (std::uint32_t i = 0; i < splitNodeCount; ++i)
    {
        MoBVHSplitNode& splitNode = const_cast<MoBVHSplitNode&>(bvh->pSplitNodes[i]);
        splitNode.start = splitNode.offset == 0 ? indices[splitNode.start] : 0;
    }

    carray_resize(&bvh->pSplitNodes, &bvh->splitNodeCount, splitNodeCount);
    carray_free(entries, &entryCount);
    carray_free(indices, &currentSize);
}

void moDestroyBVH(MoBVH bvh)
{
    carray_free(bvh->pSplitNodes, &bvh->splitNodeCount);
    carray_free(bvh->pTriangles, &bvh->triangleCount);
    delete bvh;
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
