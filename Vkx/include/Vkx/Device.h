#pragma once

#if !defined(VKX_DEVICE_H)
#define VKX_DEVICE_H

#include <vulkan/vulkan.hpp>

namespace Vkx
{

class Device
{
    Device(vk::PhysicalDevice & physicalDevice, vk::DeviceCreateInfo const & info);
    ~Device();

private:

    vk::Device device_;
};

} // namespace Vkx

#endif // !defined(VKX_DEVICE_H)