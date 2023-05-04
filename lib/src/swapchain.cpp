//
// Created by x on 2/18/23.
//

#include "swapchain.hpp"
#include "utils.hpp"

#include <limits>
#include <iostream>

namespace rcc {

Swapchain::Swapchain(vk::Device &device,
                     vk::PhysicalDevice &physical_device,
                     vk::SurfaceKHR &surface_khr,
                     VmaAllocator &allocator_ref,
                     vk::Extent2D windowExtent) :
    logicalDevice{device},
    physicalDevice{physical_device},
    windowExtent{windowExtent},
    surface{surface_khr},
    allocator{allocator_ref} {
    init();
}

Swapchain::Swapchain(vk::Device &device,
                     vk::PhysicalDevice &physical_device,
                     vk::SurfaceKHR &surface_khr,
                     VmaAllocator &allocator_ref,
                     vk::Extent2D windowExtent,
                     std::shared_ptr<Swapchain> previous) :
    logicalDevice{device},
    physicalDevice{physical_device},
    windowExtent{windowExtent},
    surface{surface_khr},
    allocator{allocator_ref},
    oldSwapchain{previous} {
    init();
}

void Swapchain::init() {
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDepthRecourses();
    createOffscreenFramebuffer();
    createPresentationFramebuffers();
}

Swapchain::~Swapchain() {

    for (auto &imageView : swapchainImageViews) logicalDevice.destroy(imageView);
    swapchainImageViews.clear();

    logicalDevice.destroy(offscreen_framebuffer.position.view);
    logicalDevice.destroy(offscreen_framebuffer.normal.view);
    logicalDevice.destroy(offscreen_framebuffer.albedo.view);
    logicalDevice.destroy(offscreen_framebuffer.depth.view);
    vmaDestroyImage(allocator,
                    offscreen_framebuffer.position.image.image,
                    offscreen_framebuffer.position.image.allocation);
    vmaDestroyImage(allocator, offscreen_framebuffer.normal.image.image, offscreen_framebuffer.normal.image.allocation);
    vmaDestroyImage(allocator, offscreen_framebuffer.albedo.image.image, offscreen_framebuffer.albedo.image.allocation);
    vmaDestroyImage(allocator, offscreen_framebuffer.depth.image.image, offscreen_framebuffer.depth.image.allocation);

    logicalDevice.destroy(depthImageView);
    vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);

    logicalDevice.destroy(swapchain);
    swapchainImages.clear();

    for (auto &framebuffer : framebuffers) logicalDevice.destroy(framebuffer);
    framebuffers.clear();
    logicalDevice.destroy(finalRenderPass);

    logicalDevice.destroy(offscreen_framebuffer.framebuffer);
    logicalDevice.destroy(offscreen_framebuffer.renderPass);
    logicalDevice.destroy(g_buffer_sampler);
}

SwapchainSupportCapabilities Swapchain::querySwapchainCapabilities() {
    SwapchainSupportCapabilities capabilities;
    capabilities.surface_capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    capabilities.surface_formats = physicalDevice.getSurfaceFormatsKHR(surface);
    capabilities.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return capabilities;
}

vk::SurfaceFormatKHR Swapchain::chooseSwapchainSurfaceFormat(const SwapchainSupportCapabilities &capabilities) {
    const auto &availableFormats = capabilities.surface_formats;

    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format==vk::Format::eR8G8B8A8Srgb
            && availableFormat.colorSpace==vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR Swapchain::choosePresentMode(const SwapchainSupportCapabilities &capabilities) {
    const auto &availablePresentModes = capabilities.presentModes;

    // mailbox present mode has lower latency and higher power consumption, because unnecessary work is done when rendering faster than monitor framerate
    for (const auto &availablePresentMode : availablePresentModes) {
        if (availablePresentMode==vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }

    // this corresponds to VSYNC and is always enabled
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Swapchain::chooseSwapchainExtent(const SwapchainSupportCapabilities &capabilities,
                                              vk::Extent2D windowExtent) {
    const vk::Extent2D &currentExtent = capabilities.surface_capabilities.currentExtent;
    const vk::Extent2D &minExtent = capabilities.surface_capabilities.minImageExtent;
    const vk::Extent2D &maxExtent = capabilities.surface_capabilities.maxImageExtent;


    // equivalent to =! -1
    if (currentExtent.width!=std::numeric_limits<uint32_t>::max()) {
        return currentExtent;
    } else {
        // clamp Extent to min and max swapchain extent
        VkExtent2D actualExtent = windowExtent;
        actualExtent.width = std::max(
            minExtent.width,
            std::min(maxExtent.width, actualExtent.width));

        actualExtent.height = std::max(
            minExtent.height,
            std::min(maxExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Swapchain::createSwapchain() {
    auto capabilities = querySwapchainCapabilities();
    auto surfaceFormat = chooseSwapchainSurfaceFormat(capabilities);
    auto presentMode = choosePresentMode(capabilities);
    auto extent = chooseSwapchainExtent(capabilities, windowExtent);

    uint32_t imageCount = capabilities.surface_capabilities.minImageCount + 1;
    if (capabilities.surface_capabilities.maxImageCount!=0) {
        imageCount = std::min(capabilities.surface_capabilities.maxImageCount, imageCount);
    }

    vk::SwapchainCreateInfoKHR swapchainInfo{
        vk::SwapchainCreateFlagsKHR(),
        surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive, // TODO: Update for multiple queues (compare to the old rcc project)
        nullptr, // TODO: Update for multiple queues
        vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        presentMode,
        true,
        (oldSwapchain) ? oldSwapchain->swapchain : nullptr
    };

    if (oldSwapchain) {
        oldSwapchain.reset();
    }

    static int temp = 0;
    temp++;

    swapchain = logicalDevice.createSwapchainKHR(swapchainInfo);
    swapchainImages = logicalDevice.getSwapchainImagesKHR(swapchain);
    imageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void Swapchain::createImageViews() {

    swapchainImageViews.resize(swapchainImages.size());

    vk::ImageViewCreateInfo imageViewInfo{
        vk::ImageViewCreateFlags(),
        nullptr,
        vk::ImageViewType::e2D,
        imageFormat,
        {},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };

    for (int i = 0; i < swapchainImages.size(); i++) {
        imageViewInfo.image = swapchainImages[i];
        swapchainImageViews[i] = logicalDevice.createImageView(imageViewInfo);
    }

}

void Swapchain::createRenderPass() {

    //color attachment
    vk::AttachmentDescription colorAttachmentDescription{
        vk::AttachmentDescriptionFlags(),
        imageFormat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR};
    vk::AttachmentReference colorReference{0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::AttachmentDescription depth_attachment_description{
        vk::AttachmentDescriptionFlags(),
        chooseDepthFormat(),
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal};
    vk::AttachmentReference depth_reference{1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpassDescription{
        vk::SubpassDescriptionFlags(),
        vk::PipelineBindPoint::eGraphics,
        {}, colorReference, {}, &depth_reference, {}
    };

    std::array<vk::AttachmentDescription, 2> attachments{
        colorAttachmentDescription, depth_attachment_description};

    // TODO CHECK IF SUBPASS DEPENDENCIES WERE REALLY UNNECESSARY
    // Check Out: https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/

    vk::RenderPassCreateInfo renderPassInfo{
        vk::RenderPassCreateFlags(), attachments, subpassDescription, nullptr};
    finalRenderPass = logicalDevice.createRenderPass(renderPassInfo);
}

void Swapchain::createPresentationFramebuffers() {
    framebuffers.resize(swapchainImages.size());

    for (int i = 0; i < imageCount(); i++) {
        std::array<vk::ImageView, 2> attachments = {swapchainImageViews[i], depthImageView};
        vk::Extent2D extent = swapchainExtent;

        vk::FramebufferCreateInfo framebufferInfo{
            vk::FramebufferCreateFlags(),
            finalRenderPass,
            attachments,
            extent.width,
            extent.height,
            1};

        framebuffers[i] = logicalDevice.createFramebuffer(framebufferInfo);
    }

}
void Swapchain::createDepthRecourses() {
    depthFormat = chooseDepthFormat();
    vk::Extent3D extent = {swapchainExtent.width, swapchainExtent.height, 1};

    auto depth_image_create_info = static_cast<VkImageCreateInfo>(
        imageCreateInfo(depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment, extent));

    VmaAllocationCreateInfo depth_image_alloc_info = {};
    depth_image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    depth_image_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &depth_image_create_info, &depth_image_alloc_info,
                   &depthImage.image, &depthImage.allocation, nullptr);

    VkImageViewCreateInfo depth_image_view_create_info = static_cast<VkImageViewCreateInfo>(imageviewCreateInfo(
        depthFormat,
        depthImage.image,
        vk::ImageAspectFlagBits::eDepth));
    depthImageView = logicalDevice.createImageView(depth_image_view_create_info);

}

vk::Format Swapchain::chooseDepthFormat() {
    std::vector<vk::Format>
        candidates{vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint};
    for (vk::Format format : candidates) {
        auto formatProperties = physicalDevice.getFormatProperties(format);
        // check if the candidate has optimal tiling features
        if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            ==vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    std::cerr << "No appropriate depth format" << std::endl;
    abort();
}

std::pair<vk::Result, uint32_t> Swapchain::acquireNextImage(vk::Semaphore signalOnAcquire) {
    uint32_t acquiredIndex = -1;

    // we use the C version as vulkan_hpp throws an exception at VK_SWAPCHAIN_OUT_OF_DATE_KHR
    auto result = static_cast<vk::Result>(vkAcquireNextImageKHR(logicalDevice,
                                                                swapchain,
                                                                1'000'000'000,
                                                                signalOnAcquire,
                                                                nullptr,
                                                                &acquiredIndex));
    return {result, acquiredIndex};
}

vk::Result Swapchain::present(uint32_t swapchainIndex, vk::Semaphore renderCompleted, vk::Queue presentQueue) {
    vk::PresentInfoKHR present_info{renderCompleted, swapchain, swapchainIndex};
    // we use the C version as vulkan_hpp throws an exception at VK_SWAPCHAIN_OUT_OF_DATE_KHR
    return static_cast<vk::Result>(vkQueuePresentKHR(presentQueue,
                                                     reinterpret_cast<VkPresentInfoKHR *>(&present_info)));
}

void Swapchain::createFramebufferAttachment(FramebufferAttachment &attachment,
                                            vk::Format format,
                                            vk::ImageUsageFlags usage) {
    attachment.format = format;
    auto aspectMask = vk::ImageAspectFlags();
    vk::ImageLayout imageLayout;

    // depth attachment
    if (usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (hasStencilComponent(format)) {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
        imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }

    // color attachment
    if (usage & vk::ImageUsageFlagBits::eColorAttachment) {
        aspectMask = vk::ImageAspectFlagBits::eColor;
        imageLayout = vk::ImageLayout::eColorAttachmentOptimal; // WHY IS THIS IN THE SASCHA WILLEMS SAMPLES?
    }

    assert(aspectMask!=vk::ImageAspectFlags());

    usage |= vk::ImageUsageFlagBits::eSampled;

    VkImageCreateInfo image_create_info = imageCreateInfo(format, usage, vk::Extent3D{swapchainExtent, 1});
    VmaAllocationCreateInfo image_alloc_info = {};
    image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator,
                   &image_create_info,
                   &image_alloc_info,
                   &attachment.image.image,
                   &attachment.image.allocation,
                   nullptr);

    vk::ImageViewCreateInfo image_view_create_info = imageviewCreateInfo(format, attachment.image.image, aspectMask);
    attachment.view = logicalDevice.createImageView(image_view_create_info);
}

bool Swapchain::hasStencilComponent(vk::Format format) {
    return static_cast<VkFormat>(format) >= VK_FORMAT_D16_UNORM_S8_UINT;
}

void Swapchain::createOffscreenFramebuffer() {
    createFramebufferAttachment(offscreen_framebuffer.position,
                                vk::Format::eR16G16B16A16Sfloat,
                                vk::ImageUsageFlagBits::eColorAttachment);
    createFramebufferAttachment(offscreen_framebuffer.normal,
                                vk::Format::eR16G16B16A16Sfloat,
                                vk::ImageUsageFlagBits::eColorAttachment);
    createFramebufferAttachment(offscreen_framebuffer.albedo,
                                vk::Format::eR8G8B8A8Unorm,
                                vk::ImageUsageFlagBits::eColorAttachment);

    depthFormat = chooseDepthFormat();
    createFramebufferAttachment(offscreen_framebuffer.depth,
                                depthFormat,
                                vk::ImageUsageFlagBits::eDepthStencilAttachment);

    std::array<vk::AttachmentDescription, 4> descriptions = {};

    // depth stencil must be attachment number 4 !!!
    for (int i = 0; i < 4; i++) {
        descriptions[i] = vk::AttachmentDescription{
            vk::AttachmentDescriptionFlags(),
            {}, // format is set later
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            (i!=3) ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eDepthStencilAttachmentOptimal
        };
    }

    descriptions[0].format = offscreen_framebuffer.position.format;
    descriptions[1].format = offscreen_framebuffer.normal.format;
    descriptions[2].format = offscreen_framebuffer.albedo.format;
    descriptions[3].format = offscreen_framebuffer.depth.format;

    std::array<vk::AttachmentReference, 3> colorReferences = {
        vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal},
        vk::AttachmentReference{1, vk::ImageLayout::eColorAttachmentOptimal},
        vk::AttachmentReference{2, vk::ImageLayout::eColorAttachmentOptimal}
    };

    vk::AttachmentReference depthReference =
        vk::AttachmentReference{3, vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpassDescription{
        vk::SubpassDescriptionFlags(),
        vk::PipelineBindPoint::eGraphics,
        {}, colorReferences, {}, &depthReference, {}
    };

    // this is for the layout transitions
    std::array<vk::SubpassDependency, 2> dependencies = {
        vk::SubpassDependency{
            VK_SUBPASS_EXTERNAL,
            0,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eMemoryRead,
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
            vk::DependencyFlagBits::eByRegion
        },
        vk::SubpassDependency{
            0,
            VK_SUBPASS_EXTERNAL,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
            vk::AccessFlagBits::eMemoryRead,
            vk::DependencyFlagBits::eByRegion
        }
    };

    vk::RenderPassCreateInfo renderPassCreateInfo{
        vk::RenderPassCreateFlags(),
        descriptions,
        subpassDescription,
        dependencies
    };
    offscreen_framebuffer.renderPass = logicalDevice.createRenderPass(renderPassCreateInfo);

    std::array<vk::ImageView, 4> attachments = {
        offscreen_framebuffer.position.view,
        offscreen_framebuffer.normal.view,
        offscreen_framebuffer.albedo.view,
        offscreen_framebuffer.depth.view
    };

    vk::FramebufferCreateInfo framebufferCreateInfo{
        vk::FramebufferCreateFlags(),
        offscreen_framebuffer.renderPass,
        attachments,
        swapchainExtent.width, swapchainExtent.height, 1
    };
    offscreen_framebuffer.framebuffer = logicalDevice.createFramebuffer(framebufferCreateInfo);

    // create sampler for offscreen framebuffer
    vk::SamplerCreateInfo samplerCreateInfo{
        vk::SamplerCreateFlags(),
        vk::Filter::eNearest,
        vk::Filter::eNearest,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.0f,
        VK_TRUE,
        1.0f,
        VK_FALSE,
        vk::CompareOp::eNever,
        0.0f,
        1.0f,
        vk::BorderColor::eFloatOpaqueWhite,
        VK_FALSE
    };
    g_buffer_sampler = logicalDevice.createSampler(samplerCreateInfo);

}

} // namespace rcc