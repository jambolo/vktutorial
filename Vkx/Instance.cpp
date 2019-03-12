#include "Instance.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! @param  info    Creation info
Instance::Instance(vk::InstanceCreateInfo const & info)
    : vk::Instance(vk::createInstance(info))
{
}

Instance::~Instance()
{
    vk::Instance::destroy();
}
} // namespace Vkx
