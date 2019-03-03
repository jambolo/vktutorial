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

    //! Copies a buffer to this one and waits for the copy operation to finish.
    void copySynced(vk::Device         device,
                    vk::PhysicalDevice physicalDevice,
                    vk::CommandPool    commandPool,
                    vk::Queue          queue,
                    Buffer const &     src,
                    size_t             size);

    //! Implicitly converts into a vk::Buffer.
    operator vk::Buffer() { return buffer_.get(); }

    //! Returns the DeviceMemory handle.
    vk::DeviceMemory allocation() { return allocation_.get(); }

protected:

    vk::UniqueDeviceMemory allocation_; //!< Allocation
    vk::UniqueBuffer buffer_;           //!< Buffer
};

class GlobalBuffer : public Buffer
{
public:
    //! Constructor.
    GlobalBuffer() = default;

    //! Constructor.
    GlobalBuffer(vk::Device           device,
                 vk::PhysicalDevice   physicalDevice,
                 size_t               size,
                 vk::BufferUsageFlags usage,
                 void const *         src         = nullptr,
                 vk::SharingMode      sharingMode = vk::SharingMode::eExclusive);

    //! Copies memory into the buffer
    void set(vk::Device device, void const * src, size_t offset, size_t size);
};

class LocalBuffer : public Buffer
{
public:
    //! Constructor.
    LocalBuffer() = default;

    //! Constructor.
    LocalBuffer(vk::Device           device,
                vk::PhysicalDevice   physicalDevice,
                size_t               size,
                vk::BufferUsageFlags usage,
                vk::SharingMode      sharingMode = vk::SharingMode::eExclusive);

    //! Constructor.
    LocalBuffer(vk::Device           device,
                vk::PhysicalDevice   physicalDevice,
                size_t               size,
                vk::BufferUsageFlags usage,
                void const *         src,
                vk::CommandPool      commandPool,
                vk::Queue            queue,
                vk::SharingMode      sharingMode = vk::SharingMode::eExclusive);

    //! Copies data from CPU memory into the buffer
    void set(vk::Device         device,
             vk::PhysicalDevice physicalDevice,
             vk::CommandPool    commandPool,
             vk::Queue          queue,
             void const *       src,
             size_t             size);
};
} // namespace Vkx

#endif // !defined(VKX_BUFFER_H)
