//
// Created by x on 2/18/23.
//

#pragma once
#include <memory>
#include <vulkan_types.hpp>

namespace rcc {

struct SwapchainSupportCapabilities {
  vk::SurfaceCapabilitiesKHR surface_capabilities;
  std::vector<vk::SurfaceFormatKHR> surface_formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

class Swapchain {
 public:
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

  Swapchain(vk::Device &device,
            vk::PhysicalDevice &physical_device,
            vk::SurfaceKHR &surface,
            VmaAllocator &allocator_ref,
            vk::Extent2D windowExtent);
  Swapchain(vk::Device &device,
            vk::PhysicalDevice &physical_device,
            vk::SurfaceKHR &surface,
            VmaAllocator &allocator_ref,
            vk::Extent2D windowExtent,
            std::shared_ptr<Swapchain> previous);
  ~Swapchain();

  Swapchain(const Swapchain &) = delete;
  Swapchain &operator=(const Swapchain &) = delete;

  void init();
  uint32_t imageCount() { return swapchainImages.size(); }

  //start and end rendering
  std::pair<vk::Result, uint32_t> acquireNextImage(vk::Semaphore signalOnAcquire);
  vk::Result present(uint32_t swapchainIndex, vk::Semaphore renderCompleted, vk::Queue presentQueue);

  vk::RenderPass renderPass() { return finalRenderPass; }
  vk::Framebuffer framebuffer(uint32_t swapchainIndex) { return framebuffers[swapchainIndex]; }

 private:
  void createSwapchain();
  void createImageViews();
  void createRenderPass();
  void createDepthRecourses();
  void createFramebuffers();

  SwapchainSupportCapabilities querySwapchainCapabilities();

  vk::Format chooseDepthFormat();
  static vk::SurfaceFormatKHR chooseSwapchainSurfaceFormat(const SwapchainSupportCapabilities &capabilities);
  static vk::PresentModeKHR choosePresentMode(const SwapchainSupportCapabilities &capabilities, bool vsync = true);
  static vk::Extent2D chooseSwapchainExtent(const SwapchainSupportCapabilities &capabilities,
                                            vk::Extent2D windowExtent);

  vk::SwapchainKHR swapchain;
  vk::RenderPass finalRenderPass;

  std::vector<vk::Framebuffer> framebuffers;
  vk::Format imageFormat;
  std::vector<vk::Image> swapchainImages;
  std::vector<vk::ImageView> swapchainImageViews;

  //we could need more if we'd do postprocessing
  vk::Format depthFormat;
  AllocatedImage depthImage;
  vk::ImageView depthImageView;

  vk::Extent2D swapchainExtent;
  vk::Extent2D windowExtent;

  std::shared_ptr<Swapchain> oldSwapchain;
  uint32_t currentFrame = 0;

  vk::Device &logicalDevice;
  vk::PhysicalDevice &physicalDevice;
  vk::SurfaceKHR &surface;
  VmaAllocator &allocator;
};

} // namespace rcc