#pragma once

#if !defined(VKX_DEVICE_H)
#define VKX_DEVICE_H

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! A RAII extension to vk::Device.
class Device
{
public:
    //! Constructor.
    Device(vk::PhysicalDevice const & physicalDevice, vk::DeviceCreateInfo const & info);

    //! Move constructor.
    Device(Device && src);

    ~Device();

    //! Move-assignment operator
    Device & operator =(Device && rhs);

    //! Implicitly coverts to vk::Device
    operator vk::Device() const { return device_; }

private:
    // Non-copyable
    Device(Device const &) = delete;
    Device & operator =(Device const &) = delete;

    vk::Device device_;
};
} // namespace Vkx

#endif // !defined(VKX_DEVICE_H)
