#pragma once

#if !defined(VKX_BUFFER_H)
#define VKX_BUFFER_H

#include <vulkan/vulkan.hpp>
#include <Vkx/Vkx.h>
namespace Vkx
{
//! An extension of vk::Buffer that supports ownership of the memory.
//!
//! The buffer and its memory allocation (if any) are destroyed automatically when this object is destroyed.
class Buffer
{
public:
    //! Constructor.
    Buffer() = default;

    //! Constructor.
    Buffer(vk::Device              device,
           vk::PhysicalDevice      physicalDevice,
           size_t                  size,
           vk::BufferUsageFlags    usage,
           vk::MemoryPropertyFlags memoryProperties,
           vk::SharingMode         sharingMode = vk::SharingMode::eExclusive);

    //! Implicitly converts into a vk::Buffer
    operator vk::Buffer() { return buffer_.get(); }

    //! Returns the DeviceMemory handle
    vk::DeviceMemory allocation() { return allocation_.get(); }

private:
    vk::UniqueDeviceMemory allocation_;
    vk::UniqueBuffer buffer_;
};
} // namespace Vkx

#endif // !defined(VKX_BUFFER_H)


