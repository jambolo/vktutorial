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
    Image(vk::Device                  device,
          vk::PhysicalDevice          physicalDevice,
          vk::ImageCreateInfo const & info,
          vk::MemoryPropertyFlags     memoryProperties);

    //! Move constructor
    Image(Image && rhs);

    //! Destructor.
    virtual ~Image() = default;

    //! Move-assignment operator
    Image & operator =(Image && rhs);

    //! Implicitly converts into a vk::Image.
    operator vk::Image() { return image_.get(); }

    //! Returns the DeviceMemory handle.
    vk::DeviceMemory allocation() const { return allocation_.get(); }

protected:
    vk::UniqueDeviceMemory allocation_; //!< The image data
    vk::UniqueImage image_;             //!< The image
    vk::ImageCreateInfo info_;          //!< Info about the image
};

//! A Vulkan image that is visible to the CPU and is automatically kept in sync (eHostVisible | eHostCoherent).
class GlobalImage : public Image
{
public:
    //! Constructor.
    GlobalImage() = default;

    //! Constructor.
    GlobalImage(vk::Device                  device,
                vk::PhysicalDevice          physicalDevice,
                vk::ImageCreateInfo const & info,
                void const *                src = nullptr,
                size_t                      size = 0);

    //! Copies image data from CPU memory into the image.
    void set(vk::Device device, void const * src, size_t offset, size_t size);
};

//! A Vulkan image that is accessible only to the GPU.
class LocalImage : public Image
{
public:
    //! Constructor.
    LocalImage() = default;

    //! Constructor.
    LocalImage(vk::Device          device,
               vk::PhysicalDevice  physicalDevice,
               vk::ImageCreateInfo info);

    //! Constructor.
    LocalImage(vk::Device          device,
               vk::PhysicalDevice  physicalDevice,
               vk::CommandPool     commandPool,
               vk::Queue           queue,
               vk::ImageCreateInfo info,
               void const *        src,
               size_t              size);

    //! Copies data from CPU memory into the image
    void set(vk::Device         device,
             vk::PhysicalDevice physicalDevice,
             vk::CommandPool    commandPool,
             vk::Queue          queue,
             void const *       src,
             size_t             size);

private:
    void copy(vk::Device      device,
              vk::CommandPool commandPool,
              vk::Queue       queue,
              vk::Buffer      buffer);

    //! Transitions the layout of the image
    void transitionLayout(vk::Device      device,
                          vk::CommandPool commandPool,
                          vk::Queue       queue,
                          vk::ImageLayout oldLayout,
                          vk::ImageLayout newLayout);
};

class ImageView
{
public:
    ImageView(vk::Device device, vk::Image const & image, vk::Format format)
    {
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        view_ = device.createImageViewUnique(
            vk::ImageViewCreateInfo({},
                                    image,
                                    vk::ImageViewType::e2D,
                                    format,
                                    vk::ComponentMapping(),
                                    subresourceRange));
    }

private:
    vk::UniqueImageView view_;
};
}

#endif // !defined(VKX_IMAGE_H)
