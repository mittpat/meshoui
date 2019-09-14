#include "mo_pipeline.h"
#include "mo_device.h"
#include "mo_swapchain.h"

#include <cstring>

using namespace linalg;
using namespace linalg::aliases;

extern MoDevice g_Device;

void moCreatePipelineLayout(MoPipelineLayout *pPipeline)
{
    MoPipelineLayout pipeline = *pPipeline = new MoPipelineLayout_T();
    *pipeline = {};

    VkResult err;

    // uniform bindings
    {
        VkDescriptorSetLayoutBinding binding[1];
        // view position, light position
        binding[0].binding = 0;
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(g_Device->device, &info, VK_NULL_HANDLE, &pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT]);
        g_Device->pCheckVkResultFn(err);
    }

    // material bindings
    {
        VkDescriptorSetLayoutBinding binding[5];
        for (uint32_t i = 0; i < 5; ++i)
        {
            binding[i].binding = i;
            binding[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding[i].descriptorCount = 1;
            binding[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding[i].pImmutableSamplers = VK_NULL_HANDLE;
        }
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(g_Device->device, &info, VK_NULL_HANDLE, &pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT]);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetLayout descriptorSetLayout[MO_FRAME_COUNT] = {};
        for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
            descriptorSetLayout[i] = pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT];
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device->descriptorPool;
        alloc_info.descriptorSetCount = MO_FRAME_COUNT;
        alloc_info.pSetLayouts = descriptorSetLayout;
        err = vkAllocateDescriptorSets(g_Device->device, &alloc_info, pipeline->descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
    {
        moCreateBuffer(g_Device, &pipeline->uniformBuffer[i], sizeof(MoUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo bufferInfo[1] = {};
        bufferInfo[0].buffer = pipeline->uniformBuffer[i]->buffer;
        bufferInfo[0].offset = 0;
        bufferInfo[0].range = sizeof(MoUniform);

        VkWriteDescriptorSet descriptorWrite[1] = {};
        descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[0].dstSet = pipeline->descriptorSet[i];
        descriptorWrite[0].dstBinding = 0;
        descriptorWrite[0].dstArrayElement = 0;
        descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite[0].descriptorCount = 1;
        descriptorWrite[0].pBufferInfo = &bufferInfo[0];

        vkUpdateDescriptorSets(g_Device->device, 1, descriptorWrite, 0, VK_NULL_HANDLE);
    }

    {
        // model, view & projection
        VkPushConstantRange push_constants[1] = {};
        push_constants[0] = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant)};
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = countof(pipeline->descriptorSetLayout);
        layout_info.pSetLayouts = pipeline->descriptorSetLayout;
        layout_info.pushConstantRangeCount = countof(push_constants);
        layout_info.pPushConstantRanges = push_constants;
        err = vkCreatePipelineLayout(g_Device->device, &layout_info, VK_NULL_HANDLE, &pipeline->pipelineLayout);
        g_Device->pCheckVkResultFn(err);
    }
}

void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, VkPipeline *pPipeline)
{
    VkResult err;

    VkShaderModule vert_module;
    VkShaderModule frag_module;

    VkShaderModuleCreateInfo vert_info = {};
    vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_info.codeSize = pCreateInfo->vertexShaderSize;
    vert_info.pCode = pCreateInfo->pVertexShader;
    err = vkCreateShaderModule(g_Device->device, &vert_info, VK_NULL_HANDLE, &vert_module);
    g_Device->pCheckVkResultFn(err);
    VkShaderModuleCreateInfo frag_info = {};
    frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_info.codeSize = pCreateInfo->fragmentShaderSize;
    frag_info.pCode = pCreateInfo->pFragmentShader;
    err = vkCreateShaderModule(g_Device->device, &frag_info, VK_NULL_HANDLE, &frag_module);
    g_Device->pCheckVkResultFn(err);

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = vert_module;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = frag_module;
    stage[1].pName = "main";

    VkVertexInputBindingDescription binding_desc[5] = {};
    binding_desc[0] = {0, sizeof(float3), VK_VERTEX_INPUT_RATE_VERTEX };
    binding_desc[1] = {1, sizeof(float2), VK_VERTEX_INPUT_RATE_VERTEX };
    binding_desc[2] = {2, sizeof(float3), VK_VERTEX_INPUT_RATE_VERTEX };
    binding_desc[3] = {3, sizeof(float3), VK_VERTEX_INPUT_RATE_VERTEX };
    binding_desc[4] = {4, sizeof(float3), VK_VERTEX_INPUT_RATE_VERTEX };

    VkVertexInputAttributeDescription attribute_desc[5] = {};
    attribute_desc[0] = {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 };
    attribute_desc[1] = {1, binding_desc[1].binding, VK_FORMAT_R32G32_SFLOAT,    0 };
    attribute_desc[2] = {2, binding_desc[2].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 };
    attribute_desc[3] = {3, binding_desc[3].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 };
    attribute_desc[4] = {4, binding_desc[4].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 };

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = countof(binding_desc);
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = countof(attribute_desc);
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo assembly_info = {};
    assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = pCreateInfo->flags & MO_PIPELINE_FEATURE_BACKFACE_CULLING ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = pCreateInfo->flags & MO_PIPELINE_FEATURE_DEPTH_TEST ? VK_TRUE : VK_FALSE;
    depth_info.depthWriteEnable = pCreateInfo->flags & MO_PIPELINE_FEATURE_DEPTH_WRITE ? VK_TRUE : VK_FALSE;
    depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_info.depthBoundsTestEnable = VK_FALSE;
    depth_info.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = countof(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = 0;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &assembly_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = pCreateInfo->pipelineLayout;
    info.renderPass = pCreateInfo->renderPass;
    err = vkCreateGraphicsPipelines(g_Device->device, VK_NULL_HANDLE, 1, &info, VK_NULL_HANDLE, pPipeline);
    g_Device->pCheckVkResultFn(err);

    vkDestroyShaderModule(g_Device->device, frag_module, VK_NULL_HANDLE);
    vkDestroyShaderModule(g_Device->device, vert_module, VK_NULL_HANDLE);
}

void moDestroyPipelineLayout(MoPipelineLayout pipeline)
{
    vkQueueWaitIdle(g_Device->queue);
    for (size_t i = 0; i < MO_FRAME_COUNT; ++i) { moDeleteBuffer(g_Device, pipeline->uniformBuffer[i]); }
    vkDestroyDescriptorSetLayout(g_Device->device, pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT], VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(g_Device->device, pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT], VK_NULL_HANDLE);
    vkDestroyPipelineLayout(g_Device->device, pipeline->pipelineLayout, VK_NULL_HANDLE);
    pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT] = VK_NULL_HANDLE;
    pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT] = VK_NULL_HANDLE;
    pipeline->pipelineLayout = VK_NULL_HANDLE;
    memset(&pipeline->uniformBuffer, 0, sizeof(pipeline->uniformBuffer));
    memset(&pipeline->descriptorSet, 0, sizeof(pipeline->descriptorSet));
    delete pipeline;
}

void moBindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet pipelineDescriptorSet)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &pipelineDescriptorSet, 0, VK_NULL_HANDLE);
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
