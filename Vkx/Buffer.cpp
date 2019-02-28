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
} // namespace Vkx
