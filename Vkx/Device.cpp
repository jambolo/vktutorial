#include "Device.h"

#include "Device.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{

Device::Device(vk::PhysicalDevice & physicalDevice, vk::DeviceCreateInfo const & info)
{
    device_ = physicalDevice.createDevice(info);
}

Device::~Device()
{
    device_.destroy();
}

} // namespace Vkx
