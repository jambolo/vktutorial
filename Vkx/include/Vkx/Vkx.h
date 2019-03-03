#pragma once

#if !defined(VKX_VKX_H)
#define VKX_VKX_H

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

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

namespace ColorComponentFlags
{
    using CCF  = vk::ColorComponentFlags;                //!< @private
    using CCFB = vk::ColorComponentFlagBits;             //!< @private

    const CCF all = CCF(vk::FlagTraits<CCFB>::allFlags); //!< All colors
}

namespace DebugUtils
{
namespace MessageTypeFlags
{
    using DUMTFE  = vk::DebugUtilsMessageTypeFlagsEXT;            //!< @private
    using DUMTFBE = vk::DebugUtilsMessageTypeFlagBitsEXT;         //!< @private

    const DUMTFE all = DUMTFE(vk::FlagTraits<DUMTFBE>::allFlags); //!< All types
}

namespace MessageSeverityFlags
{
    using DUMSFE  = vk::DebugUtilsMessageSeverityFlagsEXT;        //!< @private
    using DUMSFBE = vk::DebugUtilsMessageSeverityFlagBitsEXT;     //!< @private

    const DUMSFE all = DUMSFE(vk::FlagTraits<DUMSFBE>::allFlags); //!< All severities
}
}
} // namespace Vkx

#endif // !defined(VKX_VKX_H)
