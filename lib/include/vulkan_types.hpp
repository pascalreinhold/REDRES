//
// Created by x on 1/25/23.
//

#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

struct AllocatedImage {
  VkImage image;
  VmaAllocation allocation;
};

struct UploadContext {
  vk::Fence upload_fence;
  vk::CommandPool command_pool;
  vk::CommandBuffer command_buffer;
};