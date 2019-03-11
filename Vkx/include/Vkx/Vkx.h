#pragma once

#if !defined(VKX_VKX_H)
#define VKX_VKX_H

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <functional>

//! Extensions to Vulkan
namespace Vkx
{
class Buffer;

//! Returns true if the extension is supported.
bool extensionIsSupported(std::vector<vk::ExtensionProperties> & extensions, char const * name);

//! Returns true if all extensions are supported by the device.
bool allExtensionsSupported(vk::PhysicalDevice device, std::vector<char const *> const & extensions);

//! Returns true if the layer is available.
bool layerIsAvailable(std::vector<vk::LayerProperties> layers, char const * name);

//! Returns true if all of the specified extensions are available.
bool allLayersAvailable(std::vector<char const *> const & requested);

//! Loads a shader.
vk::ShaderModule loadShaderModule(std::string const &         path,
                                  vk::Device &                device,
                                  vk::ShaderModuleCreateFlags flags = vk::ShaderModuleCreateFlags());

//! Finds an appropriate memory type provided by the physical device.
uint32_t findAppropriateMemoryType(vk::PhysicalDevice physicalDevice, uint32_t types, vk::MemoryPropertyFlags properties);

//! Creates and executes a one-time command buffer.
void executeOnceSynched(vk::Device                             device,
                        vk::CommandPool                        commandPool,
                        vk::Queue                              queue,
                        std::function<void(vk::CommandBuffer)> commands);

namespace ColorComponentFlags
{
    //! All colors
    vk::ColorComponentFlags const all = vk::ColorComponentFlags(vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags);
}

namespace DebugUtils
{
namespace MessageTypeFlags
{
    //! All message types
    vk::DebugUtilsMessageTypeFlagsEXT const all = vk::DebugUtilsMessageTypeFlagsEXT(
        vk::FlagTraits<vk::DebugUtilsMessageTypeFlagBitsEXT>::allFlags);
}

namespace MessageSeverityFlags
{
    //! All message severities
    vk::DebugUtilsMessageSeverityFlagsEXT const all = vk::DebugUtilsMessageSeverityFlagsEXT(
        vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags);                                                                                     //!<
    //!< All
    //!< severities
}
}
} // namespace Vkx

#endif // !defined(VKX_VKX_H)
