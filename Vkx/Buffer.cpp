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
//! @param  sharingMode         Sharing mode flag (default: eExclusive)
//!
//! @warning       A std::runtime_error is thrown if the buffer cannot be created and allocated
//! @todo       "... you're not supposed to actually call vkAllocateMemory for every individual buffer. ... The right way to
//!             allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a
//!             single allocation among many different objects by using the offset parameters that we've seen in many functions."

Buffer::Buffer(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               size_t                     size,
               vk::BufferUsageFlags       usage,
               vk::MemoryPropertyFlags    memoryProperties,
               vk::SharingMode            sharingMode /*= vk::SharingMode::eExclusive*/)
{
    buffer_ = device.createBufferUnique(vk::BufferCreateInfo({}, size, usage, sharingMode));

    vk::MemoryRequirements requirements = device.getBufferMemoryRequirements(buffer_.get());
    uint32_t memoryType = Vkx::findAppropriateMemoryType(physicalDevice,
                                                         requirements.memoryTypeBits,
                                                         memoryProperties);

    allocation_ = device.allocateMemoryUnique(vk::MemoryAllocateInfo(requirements.size, memoryType));
    device.bindBufferMemory(buffer_.get(), allocation_.get(), 0);
}

//! @param  src     Move source
Buffer::Buffer(Buffer && src)
{
    allocation_ = std::move(src.allocation_);
    buffer_     = std::move(src.buffer_);
}

//! @param  rhs     Move source
Buffer & Buffer::operator =(Buffer && rhs)
{
    if (this != &rhs)
    {
        allocation_ = std::move(rhs.allocation_);
        buffer_     = std::move(rhs.buffer_);
    }
    return *this;
}

//! @param  device          Logical device associated with the buffer
//! @param  physicalDevice  Physical device associated with the buffer's allocation
//! @param  size            Nominal size of the buffer
//! @param  usage           Usage flags
//! @param  src             Data to be copied into the buffer, or nullptr if nothing to copy (default: nullptr)
//! @param  sharingMode     Sharing mode flag (default: eExclusive)
HostBuffer::HostBuffer(vk::Device const &         device,
                       vk::PhysicalDevice const & physicalDevice,
                       size_t                     size,
                       vk::BufferUsageFlags       usage,
                       void const *               src /*= nullptr*/,
                       vk::SharingMode            sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage,
             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
{
    if (src)
        set(device, 0, src, size);
}

//! @param  device  Logical device associated with the buffer
//! @param  offset  Where in the buffer to put the copied data
//! @param  src     Data to be copied into the buffer
//! @param  size    Size of the data to copy
void HostBuffer::set(vk::Device const & device, size_t offset, void const * src, size_t size)
{
    char * data = (char *)device.mapMemory(allocation(), 0, size, vk::MemoryMapFlags());
    memcpy(data + offset, src, size);
    device.unmapMemory(allocation());
}

//! @param  device          Logical device associated with the buffer
//! @param  physicalDevice  Physical device associated with the buffer's allocation
//! @param  size            Nominal size of the buffer
//! @param  usage           Usage flags
//! @param  sharingMode     Sharing mode flag (default: eExclusive)
LocalBuffer::LocalBuffer(vk::Device const &         device,
                         vk::PhysicalDevice const & physicalDevice,
                         size_t                     size,
                         vk::BufferUsageFlags       usage,
                         vk::SharingMode            sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage | vk::BufferUsageFlagBits::eTransferDst,
             vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

//! @param  device          Logical device associated with the buffer
//! @param  physicalDevice  Physical device associated with the buffer's allocation
//! @param  commandPool     Command pool used to initialize the buffer
//! @param  queue           Queue used to initialize the buffer
//! @param  size            Nominal size of the buffer
//! @param  usage           Usage flags
//! @param  src             Data to be copied into the buffer
//! @param  sharingMode     Sharing mode flag (default: eExclusive)
LocalBuffer::LocalBuffer(vk::Device const &         device,
                         vk::PhysicalDevice const & physicalDevice,
                         vk::CommandPool const &    commandPool,
                         vk::Queue const &          queue,
                         size_t                     size,
                         vk::BufferUsageFlags       usage,
                         void const *               src,
                         vk::SharingMode            sharingMode /*= vk::SharingMode::eExclusive*/)
    : Buffer(device,
             physicalDevice,
             size,
             usage | vk::BufferUsageFlagBits::eTransferDst,
             vk::MemoryPropertyFlagBits::eDeviceLocal)
{
    set(device, physicalDevice, commandPool, queue, src, size);
}

//! @param  device          Logical device associated with the buffer
//! @param  physicalDevice  Physical device associated with the buffer's allocation
//! @param  commandPool     Command pool used to copy data into the buffer
//! @param  queue           Queue used to copy data into the buffer
//! @param  src             Data to be copied into the buffer
//! @param  size            Size of the data to copy
void LocalBuffer::set(vk::Device const &         device,
                      vk::PhysicalDevice const & physicalDevice,
                      vk::CommandPool const &    commandPool,
                      vk::Queue const &          queue,
                      void const *               src,
                      size_t                     size)
{
    HostBuffer staging(device,
                       physicalDevice,
                       size,
                       vk::BufferUsageFlagBits::eTransferSrc,
                       src);
    copySynched(device, physicalDevice, commandPool, queue, staging, size);
}

void LocalBuffer::copySynched(vk::Device const &         device,
                              vk::PhysicalDevice const & physicalDevice,
                              vk::CommandPool const &    commandPool,
                              vk::Queue const &          queue,
                              Buffer &                   src,
                              size_t                     size)
{
    executeOnceSynched(device, commandPool, queue, [&src, this, size] (vk::CommandBuffer commands) {
                           commands.copyBuffer(src, this->buffer_.get(), vk::BufferCopy(0, 0, size));
                       });
}
} // namespace Vkx
