#pragma once

#include <vulkan/vulkan.h>

#include "mo_buffer.h"

#define MO_FRAME_COUNT 2
#define MO_PROGRAM_DESC_LAYOUT 0
#define MO_MATERIAL_DESC_LAYOUT 1
#define MO_RENDER_DESC_LAYOUT 2
#define MO_COUNT_DESC_LAYOUT MO_RENDER_DESC_LAYOUT+1

typedef struct MoPipelineLayout_T {
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout[MO_COUNT_DESC_LAYOUT];
    // the buffers bound to this descriptor set may change frame to frame, one set per frame
    VkDescriptorSet descriptorSet[MO_FRAME_COUNT];
    MoDeviceBuffer  uniformBuffer[MO_FRAME_COUNT];
}* MoPipelineLayout;

typedef enum MoPipelineFeature {
    MO_PIPELINE_FEATURE_NONE             = 0,
    MO_PIPELINE_FEATURE_BACKFACE_CULLING = 0b0001,
    MO_PIPELINE_FEATURE_DEPTH_TEST       = 0b0010,
    MO_PIPELINE_FEATURE_DEPTH_WRITE      = 0b0100,
    MO_PIPELINE_FEATURE_DEFAULT          = MO_PIPELINE_FEATURE_BACKFACE_CULLING | MO_PIPELINE_FEATURE_DEPTH_TEST | MO_PIPELINE_FEATURE_DEPTH_WRITE,
    MO_PIPELINE_FEATURE_MAX_ENUM         = 0x7FFFFFFF
} MoPipelineFeature;
typedef VkFlags MoPipelineCreateFlags;

typedef struct MoPipelineCreateInfo {
    const uint32_t*       pVertexShader;
    uint32_t              vertexShaderSize;
    const uint32_t*       pFragmentShader;
    uint32_t              fragmentShaderSize;
    VkPipelineLayout      pipelineLayout;
    VkRenderPass          renderPass;
    MoPipelineCreateFlags flags;
} MoPipelineCreateInfo;

//create a pipeline
void moCreatePipelineLayout(MoPipelineLayout *pPipeline);
void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, VkPipeline *pPipeline);

// destroy a pipeline
void moDestroyPipelineLayout(MoPipelineLayout pipeline);

// start a new frame against the given pipeline
void moBindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet pipelineDescriptorSet);

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
