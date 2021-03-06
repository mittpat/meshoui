#pragma once

#include "mo_buffer.h"
#include "mo_device.h"
#include "mo_pipeline.h"

#include <vulkan/vulkan.h>

#include <linalg.h>

#define MO_FRAME_COUNT 2

typedef struct MoSwapChainCreateInfo {
    VkSurfaceKHR                 surface;
    VkSurfaceFormatKHR           surfaceFormat;
    VkExtent2D                   extent;
    linalg::aliases::float4      clearColor;
    VkBool32                     vsync;
    VkBool32                     offscreen;
    void                       (*pCheckVkResultFn)(VkResult err);
} MoSwapChainCreateInfo;

typedef struct MoSwapChainRecreateInfo {
    VkSurfaceKHR       surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D         extent;
    VkBool32           vsync;
    VkBool32           offscreen;
} MoSwapChainRecreateInfo;

typedef struct MoSwapChain_T {
    MoSwapBuffer    images[MO_FRAME_COUNT];
    MoCommandBuffer frames[MO_FRAME_COUNT];
    uint32_t        frameIndex;
    MoImageBuffer   depthBuffer;
    VkSwapchainKHR  swapChainKHR;
    VkRenderPass    renderPass;
    VkExtent2D      extent;
    linalg::aliases::float4 clearColor;
}* MoSwapChain;

// you can create a VkSwapchain using moCreateSwapChain(MoSwapChainCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing swap chain
void moCreateSwapChain(MoSwapChainCreateInfo* pCreateInfo, MoSwapChain* pSwapChain);
void moRecreateSwapChain(MoSwapChainRecreateInfo* pCreateInfo, MoSwapChain swapChain);
void moBeginSwapChain(MoSwapChain swapChain, MoCommandBuffer *pCurrentCommandBuffer, VkSemaphore *pImageAcquiredSemaphore);
void moBeginRenderPass(MoSwapChain swapChain, MoCommandBuffer currentCommandBuffer);
VkResult moEndSwapChain(MoSwapChain swapChain, VkSemaphore *pImageAcquiredSemaphore);

// free swap chain, command and swap buffers
void moDestroySwapChain(MoSwapChain swapChain);

typedef struct MoRenderbufferRegistration {
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet[MO_FRAME_COUNT];
} MoRenderbufferRegistration;

typedef struct MoRenderbuffer_T {
    VkSampler renderSampler;
    std::uint32_t registrationCount;
    const MoRenderbufferRegistration* pRegistrations;
}* MoRenderbuffer;

void moCreateRenderbuffer(MoRenderbuffer *pRenderbuffer);

void moReregisterRenderbuffer(MoSwapChain swapChain, MoRenderbuffer renderbuffer);

void moRegisterRenderbuffer(MoSwapChain swapChain, MoPipelineLayout pipeline, MoRenderbuffer renderbuffer);

void moDestroyRenderbuffer(MoRenderbuffer renderbuffer);

void moBindRenderbuffer(VkCommandBuffer commandBuffer, MoRenderbuffer renderbuffer, VkPipelineLayout pipelineLayout, uint32_t frameIndex);

// readback a framebuffer
void moFramebufferReadback(VkImage source, VkExtent2D extent, std::uint8_t* pDestination, uint32_t destinationSize, VkCommandPool commandPool);

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
