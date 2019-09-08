#pragma once

#include "mo_buffer.h"

#include <vulkan/vulkan.h>

#include <linalg.h>

typedef struct MoMaterial_T {
    VkSampler ambientSampler;
    VkSampler diffuseSampler;
    VkSampler normalSampler;
    VkSampler specularSampler;
    VkSampler emissiveSampler;
    // this descriptor set uses only immutable samplers, one set per swapchain
    VkDescriptorSet descriptorSet;
    MoImageBuffer ambientImage;
    MoImageBuffer diffuseImage;
    MoImageBuffer normalImage;
    MoImageBuffer specularImage;
    MoImageBuffer emissiveImage;
}* MoMaterial;

typedef struct MoTextureInfo {
    const uint8_t* pData;
    VkDeviceSize   dataSize;
    VkExtent2D     extent;
    VkFilter       filter;
    // 0 or VK_FORMAT_R8G8B8A8_UNORM for uncompressed
    VkFormat       format;
} MoTextureInfo;

typedef struct MoMaterialCreateInfo {
    linalg::aliases::float4 colorAmbient;
    linalg::aliases::float4 colorDiffuse;
    linalg::aliases::float4 colorSpecular;
    linalg::aliases::float4 colorEmissive;
    MoTextureInfo  textureAmbient;
    MoTextureInfo  textureDiffuse;
    MoTextureInfo  textureNormal;
    MoTextureInfo  textureSpecular;
    MoTextureInfo  textureEmissive;
} MoMaterialCreateInfo;

// upload a new phong material to the GPU and return a handle
void moCreateMaterial(const MoMaterialCreateInfo* pCreateInfo, MoMaterial* pMaterial);

// free a material
void moDestroyMaterial(MoMaterial material);

// bind a material
void moBindMaterial(MoMaterial material);

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
