#include "Image.h"

#include "Buffer.h"
#include "Vkx.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! @param  device              Logical device associated with the image
//! @param  physicalDevice      Physical device associated with the image's allocation
//! @param  info                Creation info
//! @param  memoryProperties    Memory properties
//!
//! @warning       A std::runtime_error is thrown if the image cannot be created and allocated
//! @todo       "... you're not supposed to actually call vkAllocateMemory for every individual image. ... The right way to
//!             allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a
//!             single allocation among many different objects by using the offset parameters that we've seen in many functions."

Image::Image(vk::Device const &          device,
             vk::PhysicalDevice const &  physicalDevice,
             vk::ImageCreateInfo const & info,
             vk::MemoryPropertyFlags     memoryProperties)
    : info_(info)
{
    image_ = device.createImageUnique(info_);

    vk::MemoryRequirements requirements = device.getImageMemoryRequirements(image_.get());
    uint32_t memoryType = findAppropriateMemoryType(physicalDevice, requirements.memoryTypeBits, memoryProperties);

    allocation_ = device.allocateMemoryUnique(vk::MemoryAllocateInfo(requirements.size, memoryType));
    device.bindImageMemory(image_.get(), allocation_.get(), 0);
}

//! @param  src     Move source
Image::Image(Image && src)
{
    allocation_ = std::move(src.allocation_);
    image_      = std::move(src.image_);
    info_       = std::move(src.info_);
}

//! @param  rhs     Move source
Image & Image::operator =(Image && rhs)
{
    if (this != &rhs)
    {
        allocation_ = std::move(rhs.allocation_);
        image_      = std::move(rhs.image_);
        info_       = std::move(rhs.info_);
    }
    return *this;
}

//! @param  device
//! @param  physicalDevice
//! @param  info
//! @param  size
//! @param  src
HostImage::HostImage(vk::Device const &          device,
                     vk::PhysicalDevice const &  physicalDevice,
                     vk::ImageCreateInfo const & info,
                     void const *                src /*= nullptr*/,
                     size_t                      size /*= 0*/)
    : Image(device,
            physicalDevice,
            info,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
{
    if (src && size > 0)
        set(device, src, 0, size);
}

//! @param  device
//! @param  src
//! @param  offset
//! @param  size
void HostImage::set(vk::Device const & device, void const * src, size_t offset, size_t size)
{
    char * data = (char *)device.mapMemory(allocation(), 0, size, vk::MemoryMapFlags());
    memcpy(data + offset, src, size);
    device.unmapMemory(allocation());
}

//! @param  device
//! @param  physicalDevice
//! @param  info
//!
//! @note       eTransferDst is automatically added to info.usage flags
LocalImage::LocalImage(vk::Device const &         device,
                       vk::PhysicalDevice const & physicalDevice,
                       vk::ImageCreateInfo        info)
    : Image(device,
            physicalDevice,
            info.setUsage(info.usage | vk::ImageUsageFlagBits::eTransferDst),
            vk::MemoryPropertyFlagBits::eDeviceLocal)
{
}

//! @param  device
//! @param  physicalDevice
//! @param  commandPool
//! @param  queue
//! @param  info
//! @param  src
//! @param  size
LocalImage::LocalImage(vk::Device const &         device,
                       vk::PhysicalDevice const & physicalDevice,
                       vk::CommandPool const &    commandPool,
                       vk::Queue const &          queue,
                       vk::ImageCreateInfo        info,
                       void const *               src,
                       size_t                     size)
    : Image(device,
            physicalDevice,
            info.setUsage(info.usage | vk::ImageUsageFlagBits::eTransferDst),
            vk::MemoryPropertyFlagBits::eDeviceLocal)
{
    set(device, physicalDevice, commandPool, queue, src, size);
}

//! @param  device
//! @param  physicalDevice
//! @param  commandPool
//! @param  queue
//! @param  src
//! @param  size
void LocalImage::set(vk::Device const &         device,
                     vk::PhysicalDevice const & physicalDevice,
                     vk::CommandPool const &    commandPool,
                     vk::Queue const &          queue,
                     void const *               src,
                     size_t                     size)
{
    transitionLayout(device,
                     commandPool,
                     queue,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eTransferDstOptimal);
    HostBuffer staging(device,
                       physicalDevice,
                       size,
                       vk::BufferUsageFlagBits::eTransferSrc,
                       src);
    copy(device, commandPool, queue, staging);
    transitionLayout(device,
                     commandPool,
                     queue,
                     vk::ImageLayout::eTransferDstOptimal,
                     vk::ImageLayout::eShaderReadOnlyOptimal);
}

void LocalImage::copy(vk::Device const &      device,
                      vk::CommandPool const & commandPool,
                      vk::Queue const &       queue,
                      vk::Buffer const &      buffer)
{
    executeOnceSynched(device,
                       commandPool,
                       queue,
                       [this, &buffer] (vk::CommandBuffer commands) {
                           vk::BufferImageCopy region(0,
                                                      0,
                                                      0,
                                                      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                                                      { 0, 0, 0 },
                                                      { info_.extent.width, info_.extent.height, 1 });
                           commands.copyBufferToImage(buffer, image_.get(), vk::ImageLayout::eTransferDstOptimal, region);
                       });
}

void LocalImage::transitionLayout(vk::Device const &      device,
                                  vk::CommandPool const & commandPool,
                                  vk::Queue const &       queue,
                                  vk::ImageLayout         oldLayout,
                                  vk::ImageLayout         newLayout)
{
    vk::PipelineStageFlags srcStage;
    vk::PipelineStageFlags dstStage;
    vk::AccessFlags        srcAccessMask;
    vk::AccessFlags        dstAccessMask;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        srcAccessMask = vk::AccessFlags();
        dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits::eShaderRead;

        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::invalid_argument("Vkx::Image::transitionLayout: unsupported layout transition");
    }

    vk::ImageMemoryBarrier barrier(srcAccessMask,
                                   dstAccessMask,
                                   oldLayout,
                                   newLayout,
                                   VK_QUEUE_FAMILY_IGNORED,
                                   VK_QUEUE_FAMILY_IGNORED,
                                   image_.get(),
                                   vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    executeOnceSynched(device,
                       commandPool,
                       queue,
                       [srcStage, dstStage, &barrier] (vk::CommandBuffer commands) {
                           commands.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
                       });
}
} // namespace Vkx
