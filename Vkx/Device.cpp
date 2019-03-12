#include "Device.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! @param  physicalDevice  Physical device to be associated with this device
//! @param  info            Creation info
Device::Device(vk::PhysicalDevice const & physicalDevice, vk::DeviceCreateInfo const & info)
{
    device_ = physicalDevice.createDevice(info);
}

//! @param  src     Move source
Device::Device(Device && src)
{
    device_ = std::move(src.device_);
}

Device::~Device()
{
    device_.destroy();
}

//! @param  rhs     Move source
Device & Device::operator =(Device && rhs)
{
    if (this != &rhs)
        device_ = std::move(rhs.device_);
    return *this;
}
} // namespace Vkx
