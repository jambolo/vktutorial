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
    Buffer(vk::Device const &         device,
           vk::PhysicalDevice const & physicalDevice,
           size_t                     size,
           vk::BufferUsageFlags       usage,
           vk::MemoryPropertyFlags    memoryProperties,
           vk::SharingMode            sharingMode = vk::SharingMode::eExclusive);

    //! Move constructor
    Buffer(Buffer && src);

    //! Destructor.
    virtual ~Buffer() = default;

    //! Move-assignment operator
    Buffer & operator =(Buffer && rhs);

    //! Implicitly converts into a vk::Buffer.
    operator vk::Buffer() const { return buffer_.get(); }

    //! Returns the DeviceMemory handle.
    vk::DeviceMemory allocation() const { return allocation_.get(); }

protected:
    vk::UniqueDeviceMemory allocation_; //!< Buffer allocation
    vk::UniqueBuffer buffer_;           //!< Vulkan buffer
};

//! A Buffer that is visible to the CPU and is automatically kept in sync (eHostVisible | eHostCoherent).
class HostBuffer : public Buffer
{
public:
    //! Constructor.
    HostBuffer() = default;

    //! Constructor.
    HostBuffer(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               size_t                     size,
               vk::BufferUsageFlags       usage,
               void const *               src         = nullptr,
               vk::SharingMode            sharingMode = vk::SharingMode::eExclusive);

    //! Copies CPU memory into the buffer
    void set(vk::Device const & device, size_t offset, void const * src, size_t size);
};

//! A Buffer that is visible only to the GPU (eDeviceLocal).
class LocalBuffer : public Buffer
{
public:
    //! Constructor.
    LocalBuffer() = default;

    //! Constructor.
    LocalBuffer(vk::Device const &         device,
                vk::PhysicalDevice const & physicalDevice,
                size_t                     size,
                vk::BufferUsageFlags       usage,
                vk::SharingMode            sharingMode = vk::SharingMode::eExclusive);

    //! Constructor.
    LocalBuffer(vk::Device const &         device,
                vk::PhysicalDevice const & physicalDevice,
                vk::CommandPool const &    commandPool,
                vk::Queue const &          queue,
                size_t                     size,
                vk::BufferUsageFlags       usage,
                void const *               src,
                vk::SharingMode            sharingMode = vk::SharingMode::eExclusive);

    //! Copies data from CPU memory into the buffer
    void set(vk::Device const &         device,
             vk::PhysicalDevice const & physicalDevice,
             vk::CommandPool const &    commandPool,
             vk::Queue const &          queue,
             void const *               src,
             size_t                     size);

private:
    void copySynched(vk::Device const &         device,
                     vk::PhysicalDevice const & physicalDevice,
                     vk::CommandPool const &    commandPool,
                     vk::Queue const &          queue,
                     Buffer &                   src,
                     size_t                     size);
};
} // namespace Vkx

#endif // !defined(VKX_BUFFER_H)
