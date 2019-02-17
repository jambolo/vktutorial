#pragma once

#if !defined(VKX_VKX_H)
#define VKX_VKX_H

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

//! Extensions to Vulkan
namespace Vkx
{
//! Returns true if the extension is supported.
bool extensionIsSupported(std::vector<vk::ExtensionProperties> & extensions, char const * name);

//! Returns true if the layer is available.
bool layerIsAvailable(std::vector<vk::LayerProperties> layers, char const * name);

//! Loads a shader.
vk::ShaderModule loadShaderModule(std::string const &         path,
                                  vk::Device &                device,
                                  vk::ShaderModuleCreateFlags flags = vk::ShaderModuleCreateFlags());
} // namespace Vkx

#endif // !defined(VKX_VKX_H)
