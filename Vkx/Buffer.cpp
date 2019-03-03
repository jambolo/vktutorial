#include "Buffer.h"

#include "Vkx.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! @param  device              Logical device associated with the buffer
//! @param  physicalDevice      Physical device associated with the buffer's allocation
//! @param  size                Nominal size of the buffer
//! @param  usage               Usage flags
//! @param  sharingMode         Sharing mode
//! @param  memoryProperties    Memory properties
//!
//! @warn       A std::runtime_error is thrown if the buffer cannot be created and allocated
//! @todo       "... you're not supposed to actually call vkAllocateMemory for every individual buffer. ... The right way to
//!             allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a
//!             single allocation among many different objects by using the offset parameters that we've seen in many functions."

Buffer::Buffer(vk::Device              device,
               vk::PhysicalDevice      physicalDevice,
               size_t                  size,
               vk::BufferUsageFlags    usage,
               vk::MemoryPropertyFlags memoryProperties,
               vk::SharingMode         sharingMode /*= vk::SharingMode::eExclusive*/)
{
    buffer_ = device.createBufferUnique(vk::BufferCreateInfo({}, size, usage, sharingMode));

    vk::MemoryRequirements requirements = device.getBufferMemoryRequirements(buffer_.get());
    uint32_t memoryType = Vkx::findAppropriateMemoryType(physicalDevice,
                                                         requirements.memoryTypeBits,
                                                         memoryProperties);

    allocation_ = device.allocateMemoryUnique(vk::MemoryAllocateInfo(requirements.size, memoryType));
    device.bindBufferMemory(buffer_.get(), allocation_.get(), 0);
}

//! @param  device
//! @param  src
//! @param  size
//! @param  commandPool
//! @param  queue
void Buffer::copySynced(vk::Device         device,
                        vk::PhysicalDevice physicalDevice,
                        vk::CommandPool    commandPool,
                        vk::Queue          queue,
                        Buffer const &     src,
                        vk::DeviceSize     size)
{
    std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(commandPool,
                                      vk::CommandBufferLevel::ePrimary,
                                      1));
    commandBuffers[0].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    commandBuffers[0].copyBuffer(src.buffer_.get(), buffer_.get(), vk::BufferCopy(0, 0, size));
    commandBuffers[0].end();
    queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &commandBuffers[0]), nullptr);
    vkQueueWaitIdle(queue);
    device.free(commandPool, 1, &commandBuffers[0]);
}

//! @param  vk::Device device
//! @param  vk::PhysicalDevice physicalDevice
//! @param  size_t size
//! @param  vk::BufferUsageFlags usage
//! @param  void const * src
//! @param  vk::SharingMode sharingMode
GlobalBuffer::GlobalBuffer(vk::Device           device,
                           vk::PhysicalDevice   physicalDevice,
                           size_t               size,
                           vk::BufferUsageFlags usage,
                           void const *         src /*= nullptr*/,
                           vk::SharingMode      sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage,
             vk::MemoryPropertyFlagBits::eHostVisible |
             vk::MemoryPropertyFlagBits::eHostCoherent)
{
    if (src)
        set(device, src, 0, size);
}

//! @param  vk::Device device
//! @param  void const * src
//! @param  size_t offset
//! @param  size_t size
void GlobalBuffer::set(vk::Device device, void const * src, size_t offset, size_t size)
{
    char * data = (char *)device.mapMemory(allocation(), 0, size, vk::MemoryMapFlags());
    memcpy(data + offset, src, size);
    device.unmapMemory(allocation());
}

//! @param  vk::Device device
//! @param  vk::PhysicalDevice physicalDevice
//! @param  vk::DeviceSize size
//! @param  vk::BufferUsageFlags usage
//! @param  vk::SharingMode sharingMode
LocalBuffer::LocalBuffer(vk::Device           device,
                         vk::PhysicalDevice   physicalDevice,
                         vk::DeviceSize       size,
                         vk::BufferUsageFlags usage,
                         vk::SharingMode      sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage | vk::BufferUsageFlagBits::eTransferDst,
             vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

//! @param  vk::Device device
//! @param  vk::PhysicalDevice physicalDevice
//! @param  vk::DeviceSize size
//! @param  vk::BufferUsageFlags usage
//! @param  void const * src
//! @param  vk::SharingMode sharingMode
LocalBuffer::LocalBuffer(vk::Device           device,
                         vk::PhysicalDevice   physicalDevice,
                         vk::DeviceSize       size,
                         vk::BufferUsageFlags usage,
                         void const *         src,
                         vk::CommandPool      commandPool,
                         vk::Queue            queue,
                         vk::SharingMode      sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage | vk::BufferUsageFlagBits::eTransferDst,
             vk::MemoryPropertyFlagBits::eDeviceLocal)
{
    set(device, physicalDevice, commandPool, queue, src, size);
}

//! @param  vk::Device device
//! @param  vk::PhysicalDevice physicalDevice
//! @param  vk::CommandPool commandPool
//! @param  vk::Queue queue
//! @param  void const * src
void LocalBuffer::set(vk::Device         device,
                      vk::PhysicalDevice physicalDevice,
                      vk::CommandPool    commandPool,
                      vk::Queue          queue,
                      void const *       src,
                      size_t             size)
{
    GlobalBuffer staging(device,
                         physicalDevice,
                         size,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         src);
    copySynced(device, physicalDevice, commandPool, queue, staging, size);
}
} // namespace Vkx
