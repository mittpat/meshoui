#include "mo_node.h"
#include "mo_array.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <experimental/filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

void moCreateNode(const aiScene* aScene, MoScene scene, aiNode* aNode, MoNode* pNode)
{
    MoNode node = *pNode = new MoNode_T();
    *node = {};

    strncpy(node->name, aNode->mName.C_Str(), sizeof(node->name));
    node->model = transpose(float4x4((float*)&aNode->mTransformation));

    carray_resize(&node->pNodes, &node->nodeCount, aNode->mNumMeshes + aNode->mNumChildren);
    for (uint32_t i = 0; i < aNode->mNumMeshes; ++i)
    {
        MoNode child = const_cast<MoNode&>(node->pNodes[i]) = new MoNode_T();
        *child = {};

        child->material = scene->pMaterials[aScene->mMeshes[aNode->mMeshes[i]]->mMaterialIndex];
        child->mesh = scene->pMeshes[aNode->mMeshes[i]];

        strncpy(child->name, aScene->mMeshes[aNode->mMeshes[i]]->mName.C_Str(), sizeof(child->name));
        child->model = identity;
    }

    for (uint32_t i = 0; i < aNode->mNumChildren; ++i)
    {
        moCreateNode(aScene, scene, aNode->mChildren[i], const_cast<MoNode*>(&node->pNodes[aNode->mNumMeshes + i]));
    }
}

void moDestroyNode(MoNode node)
{
    for (std::uint32_t i = 0; i < node->nodeCount; ++i)
    {
        moDestroyNode(node->pNodes[i]);
    }
    carray_free(node->pNodes, &node->nodeCount);
    delete node;
}

void moCreateScene(const char *filename, MoScene* pScene)
{
    MoScene scene = *pScene = new MoScene_T();
    *scene = {};

    if (strlen(filename) != 0 && std::filesystem::exists(filename))
    {
        Assimp::Importer importer;
        const aiScene * aScene = importer.ReadFile(filename, aiProcess_Debone | aiProcessPreset_TargetRealtime_Fast);
        std::filesystem::path parentdirectory = std::filesystem::path(filename).parent_path();

        carray_resize(&scene->pMaterials, &scene->materialCount, aScene->mNumMaterials);
        for (uint32_t materialIdx = 0; materialIdx < aScene->mNumMaterials; ++materialIdx)
        {
            auto* material = aScene->mMaterials[materialIdx];

            MoMaterialCreateInfo materialInfo = {};
            std::vector<std::pair<const char*, float4*>> colorMappings =
            {{std::array<const char*,3>{AI_MATKEY_COLOR_AMBIENT}[0], &materialInfo.colorAmbient},
             {std::array<const char*,3>{AI_MATKEY_COLOR_DIFFUSE}[0], &materialInfo.colorDiffuse},
             {std::array<const char*,3>{AI_MATKEY_COLOR_SPECULAR}[0], &materialInfo.colorSpecular},
             {std::array<const char*,3>{AI_MATKEY_COLOR_EMISSIVE}[0], &materialInfo.colorEmissive}};
            for (auto mapping : colorMappings)
            {
                aiColor3D color(0.f,0.f,0.f);
                aScene->mMaterials[materialIdx]->Get(mapping.first, 0, 0, color);
                *mapping.second = {color.r, color.g, color.b, 1.0f};
            }

            std::vector<std::pair<aiTextureType, MoTextureInfo*>> textureMappings =
            {{aiTextureType_AMBIENT, &materialInfo.textureAmbient},
             {aiTextureType_DIFFUSE, &materialInfo.textureDiffuse},
             {aiTextureType_SPECULAR, &materialInfo.textureSpecular},
             {aiTextureType_EMISSIVE, &materialInfo.textureEmissive},
             {aiTextureType_NORMALS, &materialInfo.textureNormal}};
            for (auto mapping : textureMappings)
            {
                aiString path;
                if (AI_SUCCESS == material->GetTexture(mapping.first, 0, &path))
                {
                    std::filesystem::path filename = parentdirectory / path.C_Str();
                    if (std::filesystem::exists(filename))
                    {
                        int x, y, n;
                        mapping.second->pData = stbi_load(filename.c_str(), &x, &y, &n, STBI_rgb_alpha);
                        mapping.second->extent = {(uint32_t)x, (uint32_t)y};
                    }
                }
            }
            moCreateMaterial(&materialInfo, const_cast<MoMaterial*>(&scene->pMaterials[materialIdx]));
        }

        carray_resize(&scene->pMeshes, &scene->meshCount, aScene->mNumMeshes);
        for (uint32_t meshIdx = 0; meshIdx < aScene->mNumMeshes; ++meshIdx)
        {
            const auto* mesh = aScene->mMeshes[meshIdx];

            std::vector<uint32_t> indices;
            for (uint32_t faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx)
            {
                const auto* face = &mesh->mFaces[faceIdx];
                switch(face->mNumIndices)
                {
                case 1:
                    break;
                case 2:
                    break;
                case 3:
                {
                    for(uint32_t numIndex = 0; numIndex < face->mNumIndices; numIndex++)
                    {
                        indices.push_back(face->mIndices[numIndex]);
                    }
                    break;
                }
                default:
                    break;
                }
            }

            std::vector<float3> vertices, normals, tangents, bitangents;
            std::vector<float2> textureCoords;
            vertices.resize(mesh->mNumVertices);
            textureCoords.resize(mesh->mNumVertices);
            normals.resize(mesh->mNumVertices);
            tangents.resize(mesh->mNumVertices);
            bitangents.resize(mesh->mNumVertices);
            for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
            {
                vertices[vertexIndex] = float3((float*)&mesh->mVertices[vertexIndex]);
                if (mesh->HasTextureCoords(0)) { textureCoords[vertexIndex] = float2((float*)&mesh->mTextureCoords[0][vertexIndex]); }
                if (mesh->mNormals) { normals[vertexIndex] = float3((float*)&mesh->mNormals[vertexIndex]); }
                if (mesh->mTangents) { tangents[vertexIndex] = float3((float*)&mesh->mTangents[vertexIndex]); }
                if (mesh->mBitangents) { bitangents[vertexIndex] = float3((float*)&mesh->mBitangents[vertexIndex]); }
            }

            MoMeshCreateInfo meshInfo = {};
            meshInfo.indexCount = mesh->mNumFaces * 3;
            meshInfo.pIndices = indices.data();
            meshInfo.vertexCount = mesh->mNumVertices;
            meshInfo.pVertices = vertices.data();
            meshInfo.pTextureCoords = textureCoords.data();
            meshInfo.pNormals = normals.data();
            meshInfo.pTangents = tangents.data();
            meshInfo.pBitangents = bitangents.data();
            moCreateMesh(&meshInfo, const_cast<MoMesh*>(&scene->pMeshes[meshIdx]));
        }

        moCreateNode(aScene, scene, aScene->mRootNode, const_cast<MoNode*>(&scene->root));
    }
}

void moDestroyScene(MoScene scene)
{
    moDestroyNode(scene->root);
    for (std::uint32_t i = 0; i < scene->meshCount; ++i)
    {
        moDestroyMesh(scene->pMeshes[i]);
    }
    for (std::uint32_t i = 0; i < scene->materialCount; ++i)
    {
        moDestroyMaterial(scene->pMaterials[i]);
    }
    delete scene;
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
