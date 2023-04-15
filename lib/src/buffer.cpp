#include "buffer.hpp"
#include <iostream>

namespace rcc {

uint32_t ResourceManager::next_handle_ = 0;

ResourceManager::ResourceManager(vk::Device &device, VmaAllocator &allocator):
    device_{device}, allocator_{allocator} {
}

ResourceManager::~ResourceManager() {
    for (auto& [handle, buffer] : buffers_) {
        destroyBuffer(buffer);
        std::cout << "Destroyed buffer with handle: " << handle << std::endl;
    }
}

uint32_t ResourceManager::createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) {
    return createBuffer(size, buffer_usage, memory_usage, vk::SharingMode::eExclusive, {});
}

uint32_t ResourceManager::createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage,
                                       vk::SharingMode sharing_mode, const std::vector<uint32_t>& queue_family_indices){

    auto insert_result = buffers_.emplace(next_handle_, Buffer{});
    assert(insert_result.second); //check if the handle is already in use
    auto& buffer = insert_result.first->second;

    buffer.size_ = size;
    buffer.buffer_usage_ = buffer_usage;
    buffer.memory_usage_ = memory_usage;

    vk::BufferCreateInfo buffer_create_info{vk::BufferCreateFlags(), size, buffer_usage, sharing_mode, queue_family_indices};
    buffer.buffer_ = device_.createBuffer(buffer_create_info);

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.usage = memory_usage;
    vmaCreateBuffer(allocator_,
                    &static_cast<const VkBufferCreateInfo&>(buffer_create_info),
                    &static_cast<const VmaAllocationCreateInfo&>(allocation_create_info),
                    reinterpret_cast<VkBuffer*>(&buffer.buffer_), &buffer.allocation_, nullptr);

    return next_handle_++;
}


BufferResource ResourceManager::createBufferResource(uint32_t buffer_handle,
                                                     vk::DeviceSize offset,
                                                     vk::DeviceSize range,
                                                     vk::DescriptorType descriptor_type) {

    range = (range == VK_WHOLE_SIZE) ? buffers_.at(buffer_handle).size_ : range;
    auto& buffer = buffers_.at(buffer_handle);
    assert(buffer.size_ >= offset + range);

    BufferResource buffer_resource{};
    buffer_resource.handle_ = buffer_handle;
    buffer_resource.offset_ = offset;
    buffer_resource.descriptor_type_ = descriptor_type;
    buffer_resource.descriptor_buffer_info_ = vk::DescriptorBufferInfo{buffer.buffer_, offset, range};
    return buffer_resource;
}


void *ResourceManager::getMappedData(Buffer &buffer) {
    return buffer.mapped_data_;
}

void *ResourceManager::getMappedData(uint32_t buffer_handle) {
    return getMappedData(buffers_.at(buffer_handle));
}

void ResourceManager::mapBuffer(Buffer &buffer) {
    vmaMapMemory(allocator_, buffer.allocation_, &buffer.mapped_data_);
}

void ResourceManager::mapBuffer(uint32_t buffer_handle) {
    mapBuffer(buffers_.at(buffer_handle));
}

void ResourceManager::unmapBuffer(Buffer &buffer) {
    vmaUnmapMemory(allocator_, buffer.allocation_);
    buffer.mapped_data_ = nullptr;
}
void ResourceManager::unmapBuffer(uint32_t buffer_handle) {
    unmapBuffer(buffers_.at(buffer_handle));
}
void ResourceManager::destroyBuffer(uint32_t buffer_handle) {
    destroyBuffer(buffers_.at(buffer_handle));
}

void ResourceManager::destroyBuffer(Buffer &buffer) {
    vmaUnmapMemory(allocator_, buffer.allocation_);
    vmaDestroyBuffer(allocator_, buffer.buffer_, buffer.allocation_);
}

void ResourceManager::writeToBuffer(BufferResource buffer_resource, const void *data, vk::DeviceSize size) {
    Buffer& buffer = buffers_.at(buffer_resource.handle_);
    assert(buffer.mapped_data_ != nullptr);

    void* final_location = reinterpret_cast<char*>(buffer.mapped_data_) + buffer_resource.offset_;
    memcpy(final_location, data, size);
}

} // namespace rcc