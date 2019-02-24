#include "Instance.h"

#include <vulkan/vulkan.hpp>

namespace Vkx
{
Instance::Instance(vk::InstanceCreateInfo const & info)
    : vk::Instance(vk::createInstance(info))
{
}


Instance::~Instance()
{
    destroy();
}

} // namespace Vkx
