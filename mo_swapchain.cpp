#include "mo_swapchain.h"
#include "mo_array.h"

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
    if (pCreateInfo->offscreen)
    {
        swapChain->extent = pCreateInfo->extent;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            {
                VkImageCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                info.imageType = VK_IMAGE_TYPE_2D;
                info.format = pCreateInfo->surfaceFormat.format;
                info.extent = {pCreateInfo->extent.width, pCreateInfo->extent.height, 1};
                info.mipLevels = 1;
                info.arrayLayers = 1;
                info.samples = VK_SAMPLE_COUNT_1_BIT;
                info.tiling = VK_IMAGE_TILING_OPTIMAL;
                info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
                info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                err = vkCreateImage(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].back);
                pCreateInfo->device->pCheckVkResultFn(err);
            }
            {
                VkMemoryRequirements req;
                vkGetImageMemoryRequirements(pCreateInfo->device->device, swapChain->images[i].back, &req);
                VkMemoryAllocateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                info.allocationSize = req.size;
                info.memoryTypeIndex = moMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
                err = vkAllocateMemory(pCreateInfo->device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].memory);
                pCreateInfo->device->pCheckVkResultFn(err);
                err = vkBindImageMemory(pCreateInfo->device->device, swapChain->images[i].back, swapChain->images[i].memory, 0);
                pCreateInfo->device->pCheckVkResultFn(err);
            }
        }
    }
    else
    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        if (pCreateInfo->offscreen)
        {
            attachment[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        else
        {
            attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
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
    moCreateBuffer(&swapChain->depthBuffer, {swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

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

    moDeleteBuffer(swapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(swapChain->images); ++i)
    {
        vkDestroyImageView(g_Device->device, swapChain->images[i].view, VK_NULL_HANDLE);
        vkDestroyFramebuffer(g_Device->device, swapChain->images[i].front, VK_NULL_HANDLE);
        if (swapChain->swapChainKHR == VK_NULL_HANDLE) //offscreen
        {
            vkFreeMemory(g_Device->device, swapChain->images[i].memory, VK_NULL_HANDLE);
            vkDestroyImage(g_Device->device, swapChain->images[i].back, VK_NULL_HANDLE);
        }
    }
    if (swapChain->renderPass)
    {
        vkDestroyRenderPass(g_Device->device, swapChain->renderPass, VK_NULL_HANDLE);
    }

    if (pCreateInfo->offscreen)
    {
        swapChain->extent = pCreateInfo->extent;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            {
                VkImageCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                info.imageType = VK_IMAGE_TYPE_2D;
                info.format = pCreateInfo->surfaceFormat.format;
                info.extent = {pCreateInfo->extent.width, pCreateInfo->extent.height, 1};
                info.mipLevels = 1;
                info.arrayLayers = 1;
                info.samples = VK_SAMPLE_COUNT_1_BIT;
                info.tiling = VK_IMAGE_TILING_OPTIMAL;
                info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
                info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                err = vkCreateImage(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].back);
                g_Device->pCheckVkResultFn(err);
            }
            {
                VkMemoryRequirements req;
                vkGetImageMemoryRequirements(g_Device->device, swapChain->images[i].back, &req);
                VkMemoryAllocateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                info.allocationSize = req.size;
                info.memoryTypeIndex = moMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
                err = vkAllocateMemory(g_Device->device, &info, VK_NULL_HANDLE, &swapChain->images[i].memory);
                g_Device->pCheckVkResultFn(err);
                err = vkBindImageMemory(g_Device->device, swapChain->images[i].back, swapChain->images[i].memory, 0);
                g_Device->pCheckVkResultFn(err);
            }
        }
    }
    else
    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        if (pCreateInfo->offscreen)
        {
            attachment[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
        else
        {
            attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
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
    moCreateBuffer(&swapChain->depthBuffer, { swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

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

void moBeginSwapChain(MoSwapChain swapChain, MoCommandBuffer *pCurrentCommandBuffer, VkSemaphore *pImageAcquiredSemaphore)
{
    VkResult err;

    if (swapChain->swapChainKHR == VK_NULL_HANDLE) //offscreen
    {
        pImageAcquiredSemaphore = nullptr;
        *pCurrentCommandBuffer = swapChain->frames[swapChain->frameIndex];
    }
    else
    {
        // previous
        *pImageAcquiredSemaphore = swapChain->frames[swapChain->frameIndex].acquired;

        err = vkAcquireNextImageKHR(g_Device->device, swapChain->swapChainKHR, UINT64_MAX, *pImageAcquiredSemaphore, VK_NULL_HANDLE, &swapChain->frameIndex);
        g_Device->pCheckVkResultFn(err);

        // current
        *pCurrentCommandBuffer = swapChain->frames[swapChain->frameIndex];

        // wait indefinitely instead of periodically checking
        err = vkWaitForFences(g_Device->device, 1, &pCurrentCommandBuffer->fence, VK_TRUE, UINT64_MAX);
        g_Device->pCheckVkResultFn(err);

        err = vkResetFences(g_Device->device, 1, &pCurrentCommandBuffer->fence);
        g_Device->pCheckVkResultFn(err);
    }
    {
        err = vkResetCommandPool(g_Device->device, pCurrentCommandBuffer->pool, 0);
        g_Device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(pCurrentCommandBuffer->buffer, &info);
        g_Device->pCheckVkResultFn(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = swapChain->renderPass;
        info.framebuffer = swapChain->images[swapChain->frameIndex].front;
        info.renderArea.extent = swapChain->extent;
        VkClearValue clearValue[2] = {};
        clearValue[0].color = {{swapChain->clearColor.x, swapChain->clearColor.y, swapChain->clearColor.z, swapChain->clearColor.w}};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(pCurrentCommandBuffer->buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport{ 0, 0, float(swapChain->extent.width), float(swapChain->extent.height), 0.f, 1.f };
    vkCmdSetViewport(pCurrentCommandBuffer->buffer, 0, 1, &viewport);
    VkRect2D scissor{ { 0, 0 },{ swapChain->extent.width, swapChain->extent.height } };
    vkCmdSetScissor(pCurrentCommandBuffer->buffer, 0, 1, &scissor);
}

VkResult moEndSwapChain(MoSwapChain swapChain, VkSemaphore *pImageAcquiredSemaphore)
{
    VkResult err;

    MoCommandBuffer currentCommandBuffer = swapChain->frames[swapChain->frameIndex];

    vkCmdEndRenderPass(currentCommandBuffer.buffer);

    if (swapChain->swapChainKHR == VK_NULL_HANDLE) //offscreen
    {
        err = vkEndCommandBuffer(currentCommandBuffer.buffer);
        g_Device->pCheckVkResultFn(err);

        err = vkResetFences(g_Device->device, 1, &currentCommandBuffer.fence);
        g_Device->pCheckVkResultFn(err);

        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &currentCommandBuffer.buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &currentCommandBuffer.complete;

        err = vkQueueSubmit(g_Device->queue, 1, &info, currentCommandBuffer.fence);
        g_Device->pCheckVkResultFn(err);

        err = vkWaitForFences(g_Device->device, 1, &currentCommandBuffer.fence, VK_TRUE, UINT64_MAX);
        g_Device->pCheckVkResultFn(err);

        err = vkResetFences(g_Device->device, 1, &currentCommandBuffer.fence);
        g_Device->pCheckVkResultFn(err);
    }
    else
    {
        {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = pImageAcquiredSemaphore;
            info.pWaitDstStageMask = &wait_stage;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &currentCommandBuffer.buffer;
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &currentCommandBuffer.complete;

            VkResult err = vkEndCommandBuffer(currentCommandBuffer.buffer);
            g_Device->pCheckVkResultFn(err);

            err = vkQueueSubmit(g_Device->queue, 1, &info, currentCommandBuffer.fence);
            g_Device->pCheckVkResultFn(err);
        }
        {
            VkPresentInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &currentCommandBuffer.complete;
            info.swapchainCount = 1;
            info.pSwapchains = &swapChain->swapChainKHR;
            info.pImageIndices = &swapChain->frameIndex;
            err = vkQueuePresentKHR(g_Device->queue, &info);
        }
    }

    return err;
}

void moDestroySwapChain(MoDevice device, MoSwapChain swapChain)
{
    VkResult err;
    err = vkDeviceWaitIdle(device->device);
    device->pCheckVkResultFn(err);
    vkQueueWaitIdle(device->queue);
    for (uint32_t i = 0; i < countof(swapChain->frames); ++i)
    {
        vkDestroyFence(device->device, swapChain->frames[i].fence, VK_NULL_HANDLE);
        vkFreeCommandBuffers(device->device, swapChain->frames[i].pool, 1, &swapChain->frames[i].buffer);
        vkDestroyCommandPool(device->device, swapChain->frames[i].pool, VK_NULL_HANDLE);
        vkDestroySemaphore(device->device, swapChain->frames[i].acquired, VK_NULL_HANDLE);
        vkDestroySemaphore(device->device, swapChain->frames[i].complete, VK_NULL_HANDLE);
    }

    moDeleteBuffer(swapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(swapChain->images); ++i)
    {
        vkDestroyImageView(device->device, swapChain->images[i].view, VK_NULL_HANDLE);
        vkDestroyFramebuffer(device->device, swapChain->images[i].front, VK_NULL_HANDLE);
        if (swapChain->swapChainKHR == VK_NULL_HANDLE) //offscreen
        {
            vkFreeMemory(device->device, swapChain->images[i].memory, VK_NULL_HANDLE);
            vkDestroyImage(device->device, swapChain->images[i].back, VK_NULL_HANDLE);
        }
    }
    vkDestroyRenderPass(device->device, swapChain->renderPass, VK_NULL_HANDLE);
    if (swapChain->swapChainKHR != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device->device, swapChain->swapChainKHR, VK_NULL_HANDLE);
    }
}

void moFramebufferReadback(VkImage source, VkExtent2D extent, std::uint8_t* pDestination, uint32_t destinationSize, VkCommandPool commandPool)
{
    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imgCreateInfo = {};
    imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCreateInfo.extent.width = extent.width;
    imgCreateInfo.extent.height = extent.height;
    imgCreateInfo.extent.depth = 1;
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.mipLevels = 1;
    imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    VkResult err = vkCreateImage(g_Device->device, &imgCreateInfo, VK_NULL_HANDLE, &dstImage);
    g_Device->pCheckVkResultFn(err);
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(g_Device->device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = moMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memRequirements.memoryTypeBits);
    err = vkAllocateMemory(g_Device->device, &memAllocInfo, VK_NULL_HANDLE, &dstImageMemory);
    g_Device->pCheckVkResultFn(err);
    err = vkBindImageMemory(g_Device->device, dstImage, dstImageMemory, 0);
    g_Device->pCheckVkResultFn(err);
    // Do the actual blit from the offscreen image to our host visible destination image
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = commandPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer copyCmd;
    err = vkAllocateCommandBuffers(g_Device->device, &cmdBufAllocateInfo, &copyCmd);
    g_Device->pCheckVkResultFn(err);
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    err = vkBeginCommandBuffer(copyCmd, &cmdBufInfo);
    g_Device->pCheckVkResultFn(err);

    auto insertImageMemoryBarrier = [](
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkImageSubresourceRange subresourceRange)
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(
            cmdbuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, VK_NULL_HANDLE,
            0, VK_NULL_HANDLE,
            1, &imageMemoryBarrier);
    };

    // Transition destination image to transfer destination layout
    insertImageMemoryBarrier(
        copyCmd,
        dstImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // colorAttachment.image is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = extent.width;
    imageCopyRegion.extent.height = extent.height;
    imageCopyRegion.extent.depth = 1;

    vkCmdCopyImage(
        copyCmd,
        source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    insertImageMemoryBarrier(
        copyCmd,
        dstImage,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    VkResult res = vkEndCommandBuffer(copyCmd);
    g_Device->pCheckVkResultFn(err);

    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCmd;
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        res = vkCreateFence(g_Device->device, &fenceInfo, VK_NULL_HANDLE, &fence);
        g_Device->pCheckVkResultFn(err);
        res = vkQueueSubmit(g_Device->queue, 1, &submitInfo, fence);
        g_Device->pCheckVkResultFn(err);
        res = vkWaitForFences(g_Device->device, 1, &fence, VK_TRUE, UINT64_MAX);
        g_Device->pCheckVkResultFn(err);
        vkDestroyFence(g_Device->device, fence, VK_NULL_HANDLE);
    }

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource{};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout subResourceLayout;

    vkGetImageSubresourceLayout(g_Device->device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* imageData = {};
    vkMapMemory(g_Device->device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imageData);
    imageData += subResourceLayout.offset;

    for (std::uint32_t pixel = 0; pixel < destinationSize && pixel < subResourceLayout.size; pixel += 4)
    {
        pDestination[pixel + 0] = imageData[pixel + 0];
        pDestination[pixel + 1] = imageData[pixel + 1];
        pDestination[pixel + 2] = imageData[pixel + 2];
        pDestination[pixel + 3] = 255;
    }

    // Clean up resources
    vkUnmapMemory(g_Device->device, dstImageMemory);
    vkFreeMemory(g_Device->device, dstImageMemory, VK_NULL_HANDLE);
    vkDestroyImage(g_Device->device, dstImage, VK_NULL_HANDLE);
}

void moCreateRenderbuffer(MoRenderbuffer *pRenderbuffer)
{
    MoRenderbuffer renderbuffer = *pRenderbuffer = new MoRenderbuffer_T();
    *renderbuffer = {};

    VkResult err;

    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        info.minFilter = info.magFilter = VK_FILTER_NEAREST;
        err = vkCreateSampler(g_Device->device, &info, VK_NULL_HANDLE, &renderbuffer->renderSampler);
        g_Device->pCheckVkResultFn(err);
    }
}

void moRecreateRenderbuffer(MoRenderbuffer renderbuffer)
{
    carray_free(renderbuffer->pRegistrations, &renderbuffer->registrationCount);
}

void moRegisterRenderbuffer(MoSwapChain swapChain, MoPipelineLayout pipeline, MoRenderbuffer renderbuffer)
{
    MoRenderbufferRegistration registration = {};
    registration.pipelineLayout = pipeline->pipelineLayout;

    {
        VkDescriptorSetLayout descriptorSetLayout[MO_FRAME_COUNT] = {};
        for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
            descriptorSetLayout[i] = pipeline->descriptorSetLayout[MO_RENDER_DESC_LAYOUT];
        VkDescriptorSetAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = g_Device->descriptorPool;
        info.descriptorSetCount = MO_FRAME_COUNT;
        info.pSetLayouts = descriptorSetLayout;
        VkResult err = vkAllocateDescriptorSets(g_Device->device, &info, registration.descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorImageInfo imageDescriptor[MO_FRAME_COUNT] = {};
        VkWriteDescriptorSet writeDescriptor[MO_FRAME_COUNT] = {};
        for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
        {
            imageDescriptor[i].sampler = renderbuffer->renderSampler;
            imageDescriptor[i].imageView = swapChain->images[MO_FRAME_COUNT-i-1].view;
            imageDescriptor[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writeDescriptor[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptor[i].dstSet = registration.descriptorSet[i];
            writeDescriptor[i].dstBinding = 0;
            writeDescriptor[i].descriptorCount = 1;
            writeDescriptor[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptor[i].pImageInfo = &imageDescriptor[i];
        }
        vkUpdateDescriptorSets(g_Device->device, 2, writeDescriptor, 0, VK_NULL_HANDLE);
    }

    carray_push_back(&renderbuffer->pRegistrations, &renderbuffer->registrationCount, registration);
}

void moDestroyRenderbuffer(MoRenderbuffer renderbuffer)
{
    for (std::uint32_t i = 0; i < renderbuffer->registrationCount; ++i)
    {
        vkFreeDescriptorSets(g_Device->device, g_Device->descriptorPool, MO_FRAME_COUNT, renderbuffer->pRegistrations[i].descriptorSet);
    }
    vkDestroySampler(g_Device->device, renderbuffer->renderSampler, VK_NULL_HANDLE);
    carray_free(renderbuffer->pRegistrations, &renderbuffer->registrationCount);
    delete renderbuffer;
}

void moBindRenderbuffer(VkCommandBuffer commandBuffer, MoRenderbuffer renderbuffer, VkPipelineLayout pipelineLayout, std::uint32_t frameIndex)
{
    for (std::uint32_t i = 0; i < renderbuffer->registrationCount; ++i)
    {
        if (renderbuffer->pRegistrations[i].pipelineLayout == pipelineLayout)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, MO_RENDER_DESC_LAYOUT, 1, &renderbuffer->pRegistrations[i].descriptorSet[frameIndex], 0, VK_NULL_HANDLE);
            break;
        }
    }
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
