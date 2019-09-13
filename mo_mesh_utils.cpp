#include "mo_mesh_utils.h"

#include <vector>

using namespace linalg;
using namespace linalg::aliases;

void moCreateDemoCube(MoMesh *pMesh, const linalg::aliases::float3 & halfExtents)
{
    static float3 cube_positions[] = { { -halfExtents.x, -halfExtents.y, -halfExtents.z },
                                       { -halfExtents.x, -halfExtents.y,  halfExtents.z },
                                       { -halfExtents.x,  halfExtents.y, -halfExtents.z },
                                       { -halfExtents.x,  halfExtents.y,  halfExtents.z },
                                       {  halfExtents.x, -halfExtents.y, -halfExtents.z },
                                       {  halfExtents.x, -halfExtents.y,  halfExtents.z },
                                       {  halfExtents.x,  halfExtents.y, -halfExtents.z },
                                       {  halfExtents.x,  halfExtents.y,  halfExtents.z } };
    static float2 cube_texcoords[] = { { halfExtents.x, 0.0f },
                                       { 0.0f, halfExtents.x },
                                       { 0.0f, 0.0f },
                                       { halfExtents.x, halfExtents.x } };
    static float3 cube_normals[] = { { 0.0f, 1.0f, 0.0f } };
    static mat<unsigned,3,3> cube_triangles[] = { { uint3{ 2, 3, 1 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 7, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 8, 5, 7 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 6, 1, 5 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 7, 1, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 6, 8 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 2, 4, 3 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 8, 7 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 8, 6, 5 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 6, 2, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 7, 5, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 2, 6 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } } };

    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    std::vector<float3> vertexPositions;
    std::vector<float2> vertexTexcoords;
    std::vector<float3> vertexNormals;
    std::vector<float3> vertexTangents;
    std::vector<float3> vertexBitangents;
//INDICES_COUNT_FROM_ONE
    for (const auto & triangle : cube_triangles)
    {
        vertexPositions.push_back(cube_positions[triangle.x.x - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.x - 1]); vertexNormals.push_back(cube_normals[triangle.z.x - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
        vertexPositions.push_back(cube_positions[triangle.x.y - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.y - 1]); vertexNormals.push_back(cube_normals[triangle.z.y - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
        vertexPositions.push_back(cube_positions[triangle.x.z - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.z - 1]); vertexNormals.push_back(cube_normals[triangle.z.z - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
    }
    for (uint32_t & index : indices) { --index; }
    for (uint32_t index = 0; index < indices.size(); index+=3)
    {
        vertexCount += 3;

        const uint32_t v1 = indices[index+0];
        const uint32_t v2 = indices[index+1];
        const uint32_t v3 = indices[index+2];

        //discardNormals
        const float3 edge1 = vertexPositions[v2] - vertexPositions[v1];
        const float3 edge2 = vertexPositions[v3] - vertexPositions[v1];
        vertexNormals[v1] = vertexNormals[v2] = vertexNormals[v3] = normalize(cross(edge1, edge2));

        const float2 deltaUV1 = vertexTexcoords[v2] - vertexTexcoords[v1];
        const float2 deltaUV2 = vertexTexcoords[v3] - vertexTexcoords[v1];
        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            vertexTangents[v1].x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            vertexTangents[v1].y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            vertexTangents[v1].z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            vertexTangents[v1] = vertexTangents[v2] = vertexTangents[v3] = normalize(vertexTangents[v1]);
            vertexBitangents[v1].x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            vertexBitangents[v1].y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            vertexBitangents[v1].z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            vertexBitangents[v1] = vertexBitangents[v2] = vertexBitangents[v3] = normalize(vertexBitangents[v1]);
        }
    }

    MoMeshCreateInfo meshInfo = {};
    meshInfo.indexCount = (uint32_t)indices.size();
    meshInfo.pIndices = indices.data();
    meshInfo.vertexCount = vertexCount;
    meshInfo.pVertices = vertexPositions.data();
    meshInfo.pTextureCoords = vertexTexcoords.data();
    meshInfo.pNormals = vertexNormals.data();
    meshInfo.pTangents = vertexTangents.data();
    meshInfo.pBitangents = vertexBitangents.data();
    moCreateMesh(&meshInfo, pMesh);
}

void moUVSphere(uint32_t meridians, uint32_t parallels, std::vector<float3> & sphere_positions, std::vector<uint32_t> & sphere_indices)
{
    sphere_positions.push_back({0.0f, 1.0f, 0.0f});
    for (uint32_t j = 0; j < parallels - 1; ++j)
    {
        float const polar = 355.0f/113.0f * float(j+1) / float(parallels);
        float const sp = std::sin(polar);
        float const cp = std::cos(polar);
        for (uint32_t i = 0; i < meridians; ++i)
        {
            float const azimuth = 2.0f * 355.0f/113.0f * float(i) / float(meridians);
            float const sa = std::sin(azimuth);
            float const ca = std::cos(azimuth);
            float const x = sp * ca;
            float const y = cp;
            float const z = sp * sa;
            sphere_positions.push_back({x, y, z});
        }
    }
    sphere_positions.push_back({0.0f, -1.0f, 0.0f});

    for (uint32_t i = 0; i < meridians; ++i)
    {
        uint32_t const a = i + 1;
        uint32_t const b = (i + 1) % meridians + 1;
        sphere_indices.emplace_back(0);
        sphere_indices.emplace_back(b);
        sphere_indices.emplace_back(a);
    }

    for (uint32_t j = 0; j < parallels - 2; ++j)
    {
        uint32_t aStart = j * meridians + 1;
        uint32_t bStart = (j + 1) * meridians + 1;
        for (uint32_t i = 0; i < meridians; ++i)
        {
            const uint32_t a = aStart + i;
            const uint32_t a1 = aStart + (i + 1) % meridians;
            const uint32_t b = bStart + i;
            const uint32_t b1 = bStart + (i + 1) % meridians;
            sphere_indices.emplace_back(a);
            sphere_indices.emplace_back(a1);
            sphere_indices.emplace_back(b1);

            sphere_indices.emplace_back(a);
            sphere_indices.emplace_back(b1);
            sphere_indices.emplace_back(b);
        }
    }

    for (uint32_t i = 0; i < meridians; ++i)
    {
        uint32_t const a = i + meridians * (parallels - 2) + 1;
        uint32_t const b = (i + 1) % meridians + meridians * (parallels - 2) + 1;
        sphere_indices.emplace_back(sphere_positions.size() - 1);
        sphere_indices.emplace_back(a);
        sphere_indices.emplace_back(b);
    }
}

void moCreateDemoSphere(MoMesh *pMesh)
{
    static float2 sphere_texcoords[] = {{ 0.0f, 0.0f }};
    std::vector<float3> sphere_positions;
    std::vector<uint32_t> sphere_indices;
    moUVSphere(64, 32, sphere_positions, sphere_indices);
    const std::vector<float3> & sphere_normals = sphere_positions;

    std::vector<mat<unsigned,3,3>> sphere_triangles;
    for (uint32_t i = 0; i < sphere_indices.size(); i+=3)
    {
        sphere_triangles.push_back({uint3{sphere_indices[i+0], sphere_indices[i+1], sphere_indices[i+2]},
                                    uint3{0,0,0},
                                    uint3{sphere_indices[i+0], sphere_indices[i+1], sphere_indices[i+2]}});
    }

    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    std::vector<float3> vertexPositions;
    std::vector<float2> vertexTexcoords;
    std::vector<float3> vertexNormals;
    std::vector<float3> vertexTangents;
    std::vector<float3> vertexBitangents;
//INDICES_COUNT_FROM_ONE
    for (const auto & triangle : sphere_triangles)
    {
        vertexPositions.push_back(sphere_positions[triangle.x.x]);vertexTexcoords.push_back(sphere_texcoords[triangle.y.x]); vertexNormals.push_back(sphere_normals[triangle.z.x]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
        vertexPositions.push_back(sphere_positions[triangle.x.y]); vertexTexcoords.push_back(sphere_texcoords[triangle.y.y]); vertexNormals.push_back(sphere_normals[triangle.z.y]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
        vertexPositions.push_back(sphere_positions[triangle.x.z]); vertexTexcoords.push_back(sphere_texcoords[triangle.y.z]); vertexNormals.push_back(sphere_normals[triangle.z.z]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
    }
    for (uint32_t index = 0; index < indices.size(); index+=3)
    {
        vertexCount += 3;

        const uint32_t v1 = indices[index+0];
        const uint32_t v2 = indices[index+1];
        const uint32_t v3 = indices[index+2];

        const float3 edge1 = vertexPositions[v2] - vertexPositions[v1];
        const float3 edge2 = vertexPositions[v3] - vertexPositions[v1];
        const float2 deltaUV1 = vertexTexcoords[v2] - vertexTexcoords[v1];
        const float2 deltaUV2 = vertexTexcoords[v3] - vertexTexcoords[v1];
        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            vertexTangents[v1].x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            vertexTangents[v1].y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            vertexTangents[v1].z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            vertexTangents[v1] = vertexTangents[v2] = vertexTangents[v3] = normalize(vertexTangents[v1]);
            vertexBitangents[v1].x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            vertexBitangents[v1].y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            vertexBitangents[v1].z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            vertexBitangents[v1] = vertexBitangents[v2] = vertexBitangents[v3] = normalize(vertexBitangents[v1]);
        }
    }

    MoMeshCreateInfo meshInfo = {};
    meshInfo.indexCount = (uint32_t)indices.size();
    meshInfo.pIndices = indices.data();
    meshInfo.vertexCount = vertexCount;
    meshInfo.pVertices = vertexPositions.data();
    meshInfo.pTextureCoords = vertexTexcoords.data();
    meshInfo.pNormals = vertexNormals.data();
    meshInfo.pTangents = vertexTangents.data();
    meshInfo.pBitangents = vertexBitangents.data();
    moCreateMesh(&meshInfo, pMesh);
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
