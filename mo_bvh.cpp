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

// adapted from Tavian Barnes' "Fast, Branchless Ray/Bounding Box Intersections"
bool MoBBox::intersect(const MoRay& ray, float &t_near, float &t_far) const
{
    float tx1 = (min.x - ray.origin.x) * ray.oneOverDirection.x;
    float tx2 = (max.x - ray.origin.x) * ray.oneOverDirection.x;

    t_near = std::min(tx1, tx2);
    t_far = std::max(tx1, tx2);

    float ty1 = (min.y - ray.origin.y) * ray.oneOverDirection.y;
    float ty2 = (max.y - ray.origin.y) * ray.oneOverDirection.y;

    t_near = std::max(t_near, std::min(ty1, ty2));
    t_far = std::min(t_far, std::max(ty1, ty2));

    float tz1 = (min.z - ray.origin.z) * ray.oneOverDirection.z;
    float tz2 = (max.z - ray.origin.z) * ray.oneOverDirection.z;

    t_near = std::max(t_near, std::min(tz1, tz2));
    t_far = std::min(t_far, std::max(tz1, tz2));

    return t_far >= t_near;
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

// Adapted from the Möller–Trumbore intersection algorithm
bool moRayTriangleIntersect(const MoRay &ray, const MoTriangle &triangle,
                            float &t, float &u, float &v, bool backfaceCulling)
{
    const float EPSILON = 0.0000001f;
    const float3 & vertex0 = triangle.v0;
    const float3 & vertex1 = triangle.v1;
    const float3 & vertex2 = triangle.v2;
    const float3 edge1 = vertex1 - vertex0;
    const float3 edge2 = vertex2 - vertex0;
    const float3 h = linalg::cross(ray.direction, edge2);
    const float a = linalg::dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
    {
        // This ray is parallel to this triangle.
        return false;
    }

    const float f = 1.0/a;
    const float3 s = ray.origin - vertex0;
    u = f * linalg::dot(s, h);
    if (u < 0.0 || u > 1.0)
    {
        return false;
    }

    const float3 q = linalg::cross(s, edge1);
    v = f * linalg::dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0)
    {
        return false;
    }

    if (backfaceCulling)
    {
        const float3 n = linalg::cross(edge1, edge2);
        if (linalg::dot(ray.direction, n) >= 0.0)
            return false;
    }

    // At this stage we can compute t to find out where the intersection point is on the line.
    t = f * linalg::dot(edge2, q);
    if (t > EPSILON)
    {
        // ray intersection
        return true;
    }

    // This means that there is a line intersection but not a ray intersection.
    return false;
}

bool moIntersectBVH(MoBVH bvh, const MoRay& ray, MoIntersectResult& intersection, bool backfaceCulling)
{
    intersection.distance = std::numeric_limits<float>::max();
    float bbhits[4];
    std::uint32_t closer, other;

    // Working set
    struct Traversal
    {
        std::uint32_t index;
        float distance;
    };
    Traversal traversal[64];
    std::int32_t stackPtr = 0;

    traversal[stackPtr].index = 0;
    traversal[stackPtr].distance = std::numeric_limits<float>::lowest();

    while (stackPtr >= 0)
    {
        std::uint32_t index = traversal[stackPtr].index;
        float near = traversal[stackPtr].distance;
        stackPtr--;
        const MoBVHSplitNode& node = bvh->pSplitNodes[index];

        if (near > intersection.distance)
        {
            continue;
        }

        if (node.offset == 0)
        {
            float t, u, v;
            const MoTriangle& triangle = bvh->pTriangles[node.start];
            if (moRayTriangleIntersect(ray, triangle, t, u, v, backfaceCulling))
            {
                if (t <= 0.0)
                {
                    intersection.pTriangle = &triangle;
                    return true;
                }
                if (t < intersection.distance)
                {
                    intersection.pTriangle = &triangle;
                    intersection.barycentric = {1.f - u - v, u, v};
                    intersection.distance = t;
                }
            }
        }
        else
        {
            bool hitLeft =  bvh->pSplitNodes[index + 1].boundingBox.intersect(ray, bbhits[0], bbhits[1]);
            bool hitRight = bvh->pSplitNodes[index + node.offset].boundingBox.intersect(ray, bbhits[2], bbhits[3]);

            if (hitLeft && hitRight)
            {
                closer = index + 1;
                other = index + node.offset;

                if (bbhits[2] < bbhits[0])
                {
                    std::swap(bbhits[0], bbhits[2]);
                    std::swap(bbhits[1], bbhits[3]);
                    std::swap(closer, other);
                }

                ++stackPtr;
                traversal[stackPtr] = Traversal{other, bbhits[2]};
                ++stackPtr;
                traversal[stackPtr] = Traversal{closer, bbhits[0]};
            }
            else if (hitLeft)
            {
                ++stackPtr;
                traversal[stackPtr] = Traversal{index + 1, bbhits[0]};
            }
            else if (hitRight)
            {
                ++stackPtr;
                traversal[stackPtr] = Traversal{index + node.offset, bbhits[2]};
            }
        }
    }

    return intersection.distance < std::numeric_limits<float>::max();
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
