#pragma once

#include <linalg.h>
#include <iosfwd>

class MoBBox
{
public:
    MoBBox() {}
    MoBBox(const linalg::aliases::float3& min, const linalg::aliases::float3& max);
    MoBBox(const linalg::aliases::float3& point);

    std::uint32_t longestSide() const;
    void expandToInclude(const linalg::aliases::float3& point);
    void expandToInclude(const MoBBox& box);

    alignas(16) linalg::aliases::float3 min;
    alignas(16) linalg::aliases::float3 max;
};

struct MoTriangle
{
    MoBBox getBoundingBox() const
    {
        MoBBox bb(v0);
        bb.expandToInclude(v1);
        bb.expandToInclude(v2);
        return bb;
    }
    linalg::aliases::float3 getCentroid() const
    {
        return (v0 + v1 + v2) / 3.0f;
    }
    void getSurface(const linalg::aliases::float3& barycentricCoordinates, linalg::aliases::float3& point, linalg::aliases::float3& normal) const;

    alignas(16) linalg::aliases::float3 v0;
    alignas(16) linalg::aliases::float3 v1;
    alignas(16) linalg::aliases::float3 v2;
};

struct MoBVHSplitNode
{
    MoBBox        boundingBox;
    std::uint32_t start;
    std::uint32_t offset;
};

typedef struct MoBVH_T
{
    const MoTriangle*     pTriangles;
    std::uint32_t         triangleCount;
    const MoBVHSplitNode* pSplitNodes;
    std::uint32_t         splitNodeCount;
}* MoBVH;

struct aiMesh;
void moCreateBVH(const aiMesh* ai_mesh, MoBVH *pBVH);
void moDestroyBVH(MoBVH bvh);

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
