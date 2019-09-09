#include "mo_swapchain.h"

extern MoDevice g_Device;

void moCreateSwapChain(MoSwapChainCreateInfo *pCreateInfo, MoSwapChain *pSwapChain)
{
    MoSwapChain swapChain = *pSwapChain = new MoSwapChain_T();
    *swapChain = {};

    VkResult err;
    // Create command buffers
    for (uint32_t i = 0; i < countof(swapChain->frames); ++i)
    {
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = pCreateInfo->device->queueFamily;
            err = vkCreateCommandPool(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->frames[i].pool);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = swapChain->frames[i].pool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(pCreateInfo->device->device, &info, &swapChain->frames[i].buffer);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->frames[i].fence);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->frames[i].acquired);
            pCreateInfo->pCheckVkResultFn(err);
            err = vkCreateSemaphore(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->frames[i].complete);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    // Create image buffers
    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = pCreateInfo->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pCreateInfo->device->physicalDevice, pCreateInfo->surface, &cap);
        pCreateInfo->pCheckVkResultFn(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent = swapChain->extent = pCreateInfo->extent;
        }
        else
        {
            info.imageExtent = swapChain->extent = cap.currentExtent;
        }
        err = vkCreateSwapchainKHR(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->swapChainKHR);
        pCreateInfo->pCheckVkResultFn(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(pCreateInfo->device->device, swapChain->swapChainKHR, &backBufferCount, NULL);
        pCreateInfo->pCheckVkResultFn(err);
        VkImage backBuffer[MO_FRAME_COUNT] = {};
        err = vkGetSwapchainImagesKHR(pCreateInfo->device->device, swapChain->swapChainKHR, &backBufferCount, backBuffer);
        pCreateInfo->pCheckVkResultFn(err);

        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            swapChain->images[i].back = backBuffer[i];
        }
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = pCreateInfo->surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->renderPass);
        pCreateInfo->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = pCreateInfo->surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            info.image = swapChain->images[i].back;
            err = vkCreateImageView(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].view);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    // depth buffer
    moCreateBuffer(pCreateInfo->device, &swapChain->depthBuffer, {swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, swapChain->depthBuffer->view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = swapChain->renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = swapChain->extent.width;
        info.height = swapChain->extent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            attachment[0] = swapChain->images[i].view;
            err = vkCreateFramebuffer(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].front);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    swapChain->clearColor = pCreateInfo->clearColor;
}

void moRecreateSwapChain(MoSwapChainRecreateInfo *pCreateInfo, MoSwapChain swapChain)
{
    VkResult err;
    VkSwapchainKHR old_swapchain = swapChain->swapChainKHR;
    err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    moDeleteBuffer(g_Device, swapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(swapChain->images); ++i)
    {
        vkDestroyImageView(g_Device->device, swapChain->images[i].view, VK_NULL_HANDLE);
        vkDestroyFramebuffer(g_Device->device, swapChain->images[i].front, VK_NULL_HANDLE);
    }
    if (swapChain->renderPass)
    {
        vkDestroyRenderPass(g_Device->device, swapChain->renderPass, VK_NULL_HANDLE);
    }

    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = pCreateInfo->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_Device->physicalDevice, pCreateInfo->surface, &cap);
        g_Device->pCheckVkResultFn(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent = swapChain->extent = pCreateInfo->extent;
        }
        else
        {
            info.imageExtent = swapChain->extent = cap.currentExtent;
        }
        err = vkCreateSwapchainKHR(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->swapChainKHR);
        g_Device->pCheckVkResultFn(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(g_Device->device, swapChain->swapChainKHR, &backBufferCount, NULL);
        g_Device->pCheckVkResultFn(err);
        VkImage backBuffer[MO_FRAME_COUNT] = {};
        err = vkGetSwapchainImagesKHR(g_Device->device, swapChain->swapChainKHR, &backBufferCount, backBuffer);
        g_Device->pCheckVkResultFn(err);

        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            swapChain->images[i].back = backBuffer[i];
        }
    }
    if (old_swapchain)
    {
        vkDestroySwapchainKHR(g_Device->device, old_swapchain, VK_NULL_HANDLE);
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = pCreateInfo->surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->renderPass);
        g_Device->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = pCreateInfo->surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            info.image = swapChain->images[i].back;
            err = vkCreateImageView(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].view);
            g_Device->pCheckVkResultFn(err);
        }
    }

    // depth buffer
    moCreateBuffer(g_Device, &swapChain->depthBuffer, { swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, swapChain->depthBuffer->view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = swapChain->renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = swapChain->extent.width;
        info.height = swapChain->extent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            attachment[0] = swapChain->images[i].view;
            err = vkCreateFramebuffer(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].front);
            g_Device->pCheckVkResultFn(err);
        }
    }
}

void moBeginSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore)
{
    VkResult err;

    *pImageAcquiredSemaphore = swapChain->frames[*pFrameIndex].acquired;
    {
        err = vkAcquireNextImageKHR(g_Device->device, swapChain->swapChainKHR, UINT64_MAX, *pImageAcquiredSemaphore, VK_NULL_HANDLE, pFrameIndex);
        g_Device->pCheckVkResultFn(err);

        err = vkWaitForFences(g_Device->device, 1, &swapChain->frames[*pFrameIndex].fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        g_Device->pCheckVkResultFn(err);

        err = vkResetFences(g_Device->device, 1, &swapChain->frames[*pFrameIndex].fence);
        g_Device->pCheckVkResultFn(err);
    }
    {
        err = vkResetCommandPool(g_Device->device, swapChain->frames[*pFrameIndex].pool, 0);
        g_Device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(swapChain->frames[*pFrameIndex].buffer, &info);
        g_Device->pCheckVkResultFn(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = swapChain->renderPass;
        info.framebuffer = swapChain->images[*pFrameIndex].front;
        info.renderArea.extent = swapChain->extent;
        VkClearValue clearValue[2] = {};
        clearValue[0].color = {{swapChain->clearColor.x, swapChain->clearColor.y, swapChain->clearColor.z, swapChain->clearColor.w}};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(swapChain->frames[*pFrameIndex].buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport{ 0, 0, float(swapChain->extent.width), float(swapChain->extent.height), 0.f, 1.f };
    vkCmdSetViewport(swapChain->frames[*pFrameIndex].buffer, 0, 1, &viewport);
    VkRect2D scissor{ { 0, 0 },{ swapChain->extent.width, swapChain->extent.height } };
    vkCmdSetScissor(swapChain->frames[*pFrameIndex].buffer, 0, 1, &scissor);
}

VkResult moEndSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore)
{
    vkCmdEndRenderPass(swapChain->frames[*pFrameIndex].buffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = pImageAcquiredSemaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &swapChain->frames[*pFrameIndex].buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &swapChain->frames[*pFrameIndex].complete;

        VkResult err = vkEndCommandBuffer(swapChain->frames[*pFrameIndex].buffer);
        g_Device->pCheckVkResultFn(err);
        err = vkQueueSubmit(g_Device->queue, 1, &info, swapChain->frames[*pFrameIndex].fence);
        g_Device->pCheckVkResultFn(err);
    }

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &swapChain->frames[*pFrameIndex].complete;
    info.swapchainCount = 1;
    info.pSwapchains = &swapChain->swapChainKHR;
    info.pImageIndices = pFrameIndex;
    return vkQueuePresentKHR(g_Device->queue, &info);
}

void moDestroySwapChain(MoDevice device, MoSwapChain pSwapChain)
{
    VkResult err;
    err = vkDeviceWaitIdle(device->device);
    device->pCheckVkResultFn(err);
    vkQueueWaitIdle(device->queue);
    for (uint32_t i = 0; i < countof(pSwapChain->frames); ++i)
    {
        vkDestroyFence(device->device, pSwapChain->frames[i].fence, VK_NULL_HANDLE);
        vkFreeCommandBuffers(device->device, pSwapChain->frames[i].pool, 1, &pSwapChain->frames[i].buffer);
        vkDestroyCommandPool(device->device, pSwapChain->frames[i].pool, VK_NULL_HANDLE);
        vkDestroySemaphore(device->device, pSwapChain->frames[i].acquired, VK_NULL_HANDLE);
        vkDestroySemaphore(device->device, pSwapChain->frames[i].complete, VK_NULL_HANDLE);
    }

    moDeleteBuffer(device, pSwapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(pSwapChain->images); ++i)
    {
        vkDestroyImageView(device->device, pSwapChain->images[i].view, VK_NULL_HANDLE);
        vkDestroyFramebuffer(device->device, pSwapChain->images[i].front, VK_NULL_HANDLE);
    }
    vkDestroyRenderPass(device->device, pSwapChain->renderPass, VK_NULL_HANDLE);
    vkDestroySwapchainKHR(device->device, pSwapChain->swapChainKHR, VK_NULL_HANDLE);
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
