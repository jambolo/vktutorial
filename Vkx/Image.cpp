#include "Image.h"

#include "Buffer.h"
#include "Vkx.h"

#include <vulkan/vulkan.hpp>

#include <array>

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
             vk::MemoryPropertyFlags     memoryProperties,
             vk::ImageAspectFlags        aspect)
    : info_(info)
{
    image_ = device.createImageUnique(info_);

    vk::MemoryRequirements requirements = device.getImageMemoryRequirements(image_.get());
    uint32_t memoryType = findAppropriateMemoryType(physicalDevice, requirements.memoryTypeBits, memoryProperties);

    allocation_ = device.allocateMemoryUnique(vk::MemoryAllocateInfo(requirements.size, memoryType));
    device.bindImageMemory(image_.get(), allocation_.get(), 0);

    view_ = device.createImageViewUnique(
        vk::ImageViewCreateInfo({},
                                image_.get(),
                                vk::ImageViewType::e2D,
                                info_.format,
                                vk::ComponentMapping(),
                                vk::ImageSubresourceRange(aspect, 0, info_.mipLevels, 0, 1)));
}

//! @param  src     Move source
Image::Image(Image && src)
{
    info_       = std::move(src.info_);
    allocation_ = std::move(src.allocation_);
    image_      = std::move(src.image_);
    view_       = std::move(src.view_);
}

//! This function returns the number of mip levels needed to reach a 1x1 texture, assuming that the values are integers and the 
//! length of a side is computed as: Length<sub>i</sub> = Length<sub>i-1</sub> > 1 ? Length<sub>i-1</sub> / 2 : 1
//!
//! @param 	width       Width of the image
//! @param 	height      Height of the image
//!
//! @return number of levels
uint32_t Image::computeMaxMipLevels(uint32_t width, uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

//! @param  rhs     Move source
Image & Image::operator =(Image && rhs)
{
    if (this != &rhs)
    {
        info_       = std::move(rhs.info_);
        allocation_ = std::move(rhs.allocation_);
        image_      = std::move(rhs.image_);
        view_       = std::move(rhs.view_);
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
                     size_t                      size /*= 0*/,
                     vk::ImageAspectFlags        aspect /*= vk::ImageAspectFlagBits::eColor*/)
    : Image(device,
            physicalDevice,
            info,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            aspect)
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
                       vk::ImageCreateInfo        info,
                       vk::ImageAspectFlags       aspect /*= vk::ImageAspectFlagBits::eColor*/)
    : Image(device,
            physicalDevice,
            info.setUsage(info.usage | vk::ImageUsageFlagBits::eTransferDst),
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            aspect)
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
                       size_t                     size,
                       vk::ImageAspectFlags       aspect /*= vk::ImageAspectFlagBits::eColor*/)
    : Image(device,
            physicalDevice,
            info.setUsage(info.usage | vk::ImageUsageFlagBits::eTransferDst),
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            aspect)
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
    // Transition to transfer dst for copy
    transitionLayout(device,
                     commandPool,
                     queue,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eTransferDstOptimal);

    // Copy the data to the image using a staging buffer
    HostBuffer staging(device,
                       physicalDevice,
                       size,
                       vk::BufferUsageFlagBits::eTransferSrc,
                       src);
    copy(device, commandPool, queue, staging);

    // If there are mip levels, then generate them. Otherwise, just go ahead and transition the image to Shader read-only
    if (info_.mipLevels > 1)
    {
        generateMipmaps(device, physicalDevice, commandPool, queue);
    }
    else
    {
        transitionLayout(device,
                         commandPool,
                         queue,
                         vk::ImageLayout::eTransferDstOptimal,
                         vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

//! @param  device
//! @param  commandPool
//! @param  queue
//! @param  buffer
void LocalImage::copy(vk::Device const &      device,
                      vk::CommandPool const & commandPool,
                      vk::Queue const &       queue,
                      vk::Buffer const &      buffer)
{
    executeOnceSynched(device,
                       commandPool,
                       queue,
                       [this, &buffer] (vk::CommandBuffer & commands) {
                           vk::BufferImageCopy region(0,
                                                      0,
                                                      0,
                                                      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                                                      { 0, 0, 0 },
                                                      { info_.extent.width, info_.extent.height, 1 });
                           commands.copyBufferToImage(buffer, image_.get(), vk::ImageLayout::eTransferDstOptimal, region);
                       });
}

//! @param  device
//! @param  commandPool
//! @param  queue
//! @param  oldLayout
//! @param  newLayout
void LocalImage::transitionLayout(vk::Device const &      device,
                                  vk::CommandPool const & commandPool,
                                  vk::Queue const &       queue,
                                  vk::ImageLayout         oldLayout,
                                  vk::ImageLayout         newLayout)
{
    vk::AccessFlags        srcAccessMask;
    vk::AccessFlags        dstAccessMask;
    vk::PipelineStageFlags srcStage;
    vk::PipelineStageFlags dstStage;
    vk::ImageAspectFlags   aspectMask;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        srcAccessMask = vk::AccessFlags();
        dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        srcStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage      = vk::PipelineStageFlagBits::eTransfer;
        aspectMask    = vk::ImageAspectFlagBits::eColor;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits::eShaderRead;
        srcStage      = vk::PipelineStageFlagBits::eTransfer;
        dstStage      = vk::PipelineStageFlagBits::eFragmentShader;
        aspectMask    = vk::ImageAspectFlagBits::eColor;
    }
    else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        srcAccessMask = vk::AccessFlags();
        dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        srcStage   = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage   = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (info_.format == vk::Format::eD32SfloatS8Uint || info_.format == vk::Format::eD24UnormS8Uint)
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
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
                                   vk::ImageSubresourceRange(aspectMask, 0, info_.mipLevels, 0, 1));

    executeOnceSynched(device,
                       commandPool,
                       queue,
                       [srcStage, dstStage, &barrier] (vk::CommandBuffer & commands) {
                           commands.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
                       });
}

//! @param 	vk::Device const & device
//! @param 	vk::PhysicalDevice const & physicalDevice
//! @param 	vk::CommandPool const & commandPool
//! @param 	vk::Queue const & queue
//!
//! @return void
void LocalImage::generateMipmaps(vk::Device const &      device,
    vk::PhysicalDevice const & physicalDevice,
    vk::CommandPool const & commandPool,
                                 vk::Queue const &       queue)
{
    // Check if image format supports blitting with linear filtering
    vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(info_.format);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }
    executeOnceSynched(device,
                       commandPool,
                       queue,
                       [this] (vk::CommandBuffer & commands) {
                           vk::ImageMemoryBarrier barrier(vk::AccessFlags(),
                                                          vk::AccessFlags(),
                                                          vk::ImageLayout::eUndefined,
                                                          vk::ImageLayout::eUndefined,
                                                          VK_QUEUE_FAMILY_IGNORED,
                                                          VK_QUEUE_FAMILY_IGNORED,
                                                          image_.get(),
                                                          vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

                           int32_t mipWidth  = info_.extent.width;
                           int32_t mipHeight = info_.extent.height;

                           for (uint32_t i = 1; i < info_.mipLevels; i++)
                           {
                               int32_t previousWidth  = mipWidth;
                               int32_t previousHeight = mipHeight;

                               if (mipWidth > 1)
                                   mipWidth /= 2;
                               if (mipHeight > 1)
                                   mipHeight /= 2;

                               // Transition the layout for the previous mip level to transfer src
                               barrier.subresourceRange.setBaseMipLevel(i - 1);
                               barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
                               barrier.setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
                               barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
                               barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

                               commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                        vk::PipelineStageFlagBits::eTransfer,
                                                        {},
                                                        nullptr,
                                                        nullptr,
                                                        barrier);

                               // Blit the previous mip level to the current mip level
                               commands.blitImage(image_.get(),
                                                  vk::ImageLayout::eTransferSrcOptimal,
                                                  image_.get(),
                                                  vk::ImageLayout::eTransferDstOptimal,
                                                  vk::ImageBlit(
                                                      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1),
                                                      {{{ 0, 0, 0 }, { previousWidth, previousHeight, 1 } } },
                                                      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1),
                                                      {{{ 0, 0, 0 }, { mipWidth, mipHeight, 1 } } }),
                                                  vk::Filter::eLinear);

//                                // Transition the previous mip level to shader read-only now that we are done with it
//                                barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
//                                barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
//                                barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
//                                barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
//                                commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
//                                                         vk::PipelineStageFlagBits::eFragmentShader,
//                                                         {},
//                                                         nullptr,
//                                                         nullptr,
//                                                         barrier);
                           }

                           // Transition the final mip level to transfer src so that all levels can be transitioned to shader
                           // read-only in one shot
                           barrier.subresourceRange.setBaseMipLevel(info_.mipLevels - 1);
                           barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
                           barrier.setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
                           barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
                           barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

                           commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eTransfer,
                               {},
                               nullptr,
                               nullptr,
                               barrier);

                           // Transition all mip levels to shader read-only
                           barrier.subresourceRange.setBaseMipLevel(0);
                           barrier.subresourceRange.setLevelCount(info_.mipLevels);
                           barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal);
                           barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                           barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
                           barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
                           commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eFragmentShader,
                               {},
                               nullptr,
                               nullptr,
                               barrier);
    });
}

//! @param  device
//! @param  physicalDevice
//! @param  commandPool
//! @param  queue
//! @param  info
DepthImage::DepthImage(vk::Device const &         device,
                       vk::PhysicalDevice const & physicalDevice,
                       vk::CommandPool const &    commandPool,
                       vk::Queue const &          queue,
                       vk::ImageCreateInfo        info)
    : LocalImage(device, physicalDevice, info, vk::ImageAspectFlagBits::eDepth)
{
    transitionLayout(device,
                     commandPool,
                     queue,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eDepthStencilAttachmentOptimal);
}
} // namespace Vkx
