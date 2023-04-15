#pragma once

#include "vulkan_types.hpp"

#include <unordered_map>
#include <vector>

namespace rcc {

// this is an allocated buffer
class Buffer{
  public:
    Buffer() = default;
    vk::Buffer buffer_{};
  private:
    vk::DeviceSize size_{};
    VmaMemoryUsage memory_usage_{};
    vk::BufferUsageFlags buffer_usage_{};

    VmaAllocation allocation_{};
    void *mapped_data_ = nullptr; // always null if buffer not mapped

    friend class ResourceManager;
};

// points to the underlying buffer and specifies the offset and descriptor type
class BufferResource{
  public:
    BufferResource() = default;
    uint32_t handle_ = 0;
    vk::DescriptorType descriptor_type_{};
    vk::DescriptorBufferInfo descriptor_buffer_info_{};
  friend class ResourceManager;
};

class ResourceManager {
  public:
    ResourceManager(vk::Device& device, VmaAllocator& allocator);
    ~ResourceManager();

    Buffer& getBuffer(uint32_t buffer_handle);
    Buffer& getBuffer(BufferResource buffer_resource);


    // create a buffer and return a handle to it
    [[nodiscard]] uint32_t createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage);
    [[nodiscard]] uint32_t createBuffer(size_t size, vk::BufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage,
                          vk::SharingMode sharing_mode, const std::vector<uint32_t>& queue_family_indices);

    std::pair<BufferResource, BufferResource> uploadMesh(class Mesh &mesh,
                                                         UploadContext upload_context_,
                                                         vk::Queue upload_queue_);

    void stageBuffer(void *src, vk::DeviceSize size, BufferResource dest_resource, UploadContext& upload_context, vk::Queue& upload_queue);
    void immediateSubmit(UploadContext& upload_context, vk::Queue& upload_queue, std::function<void(vk::CommandBuffer cmd)> &&function);

    void writeToBuffer(BufferResource buffer_resource, const void* data, vk::DeviceSize size);
    void clearBuffer(BufferResource buffer_resource);

    void destroyBuffer(uint32_t buffer_handle);
    void destroyBuffer(Buffer& buffer);

    // create a buffer resource from a buffer handle
    [[nodiscard]] BufferResource createBufferResource(uint32_t buffer_handle, vk::DeviceSize offset, vk::DeviceSize range, vk::DescriptorType descriptor_type);
    void readFromBuffer(BufferResource &buffer, uint32_t range, void *data);
    void readFromBufferAndClearIt(BufferResource &buffer, uint32_t range, void *data);


    void mapBuffer(Buffer& buffer);
    void mapBuffer(uint32_t buffer_handle);
    static void* getMappedData(Buffer& buffer);
    void* getMappedData(uint32_t buffer_handle);
    void unmapBuffer(Buffer& buffer);
    void unmapBuffer(uint32_t buffer_handle);

    std::unordered_map<uint32_t, Buffer> buffers_;
  private:
    vk::Device& device_;
    VmaAllocator& allocator_;
    static uint32_t next_handle_;

};

} // namespace rcc

