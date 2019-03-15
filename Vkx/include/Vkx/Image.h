#if !defined(VKX_IMAGE_H)
#define VKX_IMAGE_H

#pragma once

#include <vulkan/vulkan.hpp>
#include <Vkx/Vkx.h>

namespace Vkx
{
//! An extension to vk::Image that supports ownership of the memory and the view.
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
          vk::MemoryPropertyFlags     memoryProperties,
          vk::ImageAspectFlags        aspect);

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

    //! Returns the view
    vk::ImageView view() const { return view_.get(); }

    //! Returns the creation info.
    vk::ImageCreateInfo info() const { return info_; }

    //! Returns the maximum number of mip levels needed for the given with and height.
    static uint32_t computeMaxMipLevels(uint32_t width, uint32_t height);

protected:
    vk::ImageCreateInfo info_;          //!< Info about the image
    vk::UniqueDeviceMemory allocation_; //!< The image data
    vk::UniqueImage image_;             //!< The image
    vk::UniqueImageView view_;          //!< The image view
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
              size_t                      size = 0,
              vk::ImageAspectFlags        aspect = vk::ImageAspectFlagBits::eColor);

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
               vk::ImageCreateInfo        info,
               vk::ImageAspectFlags       aspect = vk::ImageAspectFlagBits::eColor);

    //! Constructor.
    LocalImage(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               vk::CommandPool const &    commandPool,
               vk::Queue const &          queue,
               vk::ImageCreateInfo        info,
               void const *               src,
               size_t                     size,
               vk::ImageAspectFlags       aspect = vk::ImageAspectFlagBits::eColor);

    //! Copies data from CPU memory into the image
    void set(vk::Device const &         device,
             vk::PhysicalDevice const & physicalDevice,
             vk::CommandPool const &    commandPool,
             vk::Queue const &          queue,
             void const *               src,
             size_t                     size);

    void copy(vk::Device const &      device,
              vk::CommandPool const & commandPool,
              vk::Queue const &       queue,
              vk::Buffer const &      buffer);

    void transitionLayout(vk::Device const &      device,
                          vk::CommandPool const & commandPool,
                          vk::Queue const &       queue,
                          vk::ImageLayout         oldLayout,
                          vk::ImageLayout         newLayout);

    void generateMipmaps(vk::Device const &         device,
                         vk::PhysicalDevice const & physicalDevice,
                         vk::CommandPool const &    commandPool,
                         vk::Queue const &          queue);
};

//! A LocalImage for use as a depth buffer (vk::ImageAspect::eDEPTH).
class DepthImage : public LocalImage
{
public:
    //! Constructor.
    DepthImage() = default;

    //! Constructor.
    DepthImage(vk::Device const &         device,
               vk::PhysicalDevice const & physicalDevice,
               vk::CommandPool const &    commandPool,
               vk::Queue const &          queue,
               vk::ImageCreateInfo        info);
};

//! A LocalImage for use as a MSAA buffer (vk::ImageLayout::eColorAttachmentOptimal).
class ResolveImage : public LocalImage
{
public:
    //! Constructor.
    ResolveImage() = default;

    //! Constructor.
    ResolveImage(vk::Device const &         device,
                 vk::PhysicalDevice const & physicalDevice,
                 vk::CommandPool const &    commandPool,
                 vk::Queue const &          queue,
                 vk::ImageCreateInfo        info);
};
}

#endif // !defined(VKX_IMAGE_H)
