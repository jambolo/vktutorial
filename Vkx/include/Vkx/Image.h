#if !defined(VKX_IMAGE_H)
#define VKX_IMAGE_H

#pragma once

#include <vulkan/vulkan.hpp>
#include <Vkx/Vkx.h>

namespace Vkx
{
//! An extension to vk::Image that supports ownership of the memory.
//!
//! @note   Instances can be moved, but cannot be copied.
class Image
{
public:
    //! Constructor.
    Image() = default;

    //! Constructor.
    Image(vk::Device const &          device,
          vk::PhysicalDevice const &  physicalDevice,
          vk::ImageCreateInfo const & info,
          vk::MemoryPropertyFlags     memoryProperties);

    //! Move constructor
    Image(Image && src);

    //! Destructor.
    virtual ~Image() = default;

    //! Move-assignment operator
    Image & operator =(Image && rhs);

    //! Implicitly converts into a vk::Image.
    operator vk::Image() const { return image_.get(); }

    //! Returns the DeviceMemory handle.
    vk::DeviceMemory allocation() const { return allocation_.get(); }

    //! Returns the creation info.
    vk::ImageCreateInfo info() const { return info_; }

protected:
    vk::UniqueDeviceMemory allocation_; //!< The image data
    vk::UniqueImage image_;             //!< The image
    vk::ImageCreateInfo info_;          //!< Info about the image
};

//! An Image that is visible to the CPU and is automatically kept in sync (eHostVisible | eHostCoherent).
class HostImage : public Image
{
public:
    //! Constructor.
    HostImage() = default;

    //! Constructor.
    HostImage(vk::Device const &          device,
              vk::PhysicalDevice const &  physicalDevice,
              vk::ImageCreateInfo const & info,
              void const *                src = nullptr,
              size_t                      size = 0);

    //! Copies image data from CPU memory into the image.
    void set(vk::Device const & device, void const * src, size_t offset, size_t size);
};

//! An Image that is accessible only to the GPU (eDeviceLocal).
class LocalImage : public Image
{
public:
    //! Constructor.
    LocalImage() = default;

    //! Constructor.
    LocalImage(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               vk::ImageCreateInfo        info);

    //! Constructor.
    LocalImage(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               vk::CommandPool const &    commandPool,
               vk::Queue const &          queue,
               vk::ImageCreateInfo        info,
               void const *               src,
               size_t                     size);

    //! Copies data from CPU memory into the image
    void set(vk::Device const &         device,
             vk::PhysicalDevice const & physicalDevice,
             vk::CommandPool const &    commandPool,
             vk::Queue const &          queue,
             void const *               src,
             size_t                     size);

private:
    void copy(vk::Device const &      device,
              vk::CommandPool const & commandPool,
              vk::Queue const &       queue,
              vk::Buffer const &      buffer);

    //! Transitions the layout of the image
    void transitionLayout(vk::Device const &      device,
                          vk::CommandPool const & commandPool,
                          vk::Queue const &       queue,
                          vk::ImageLayout         oldLayout,
                          vk::ImageLayout         newLayout);
};
}

#endif // !defined(VKX_IMAGE_H)
