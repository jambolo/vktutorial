#include "Vkx/Vkx.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace Vkx
{
//! @param 	extensions
//! @param 	name
bool extensionIsSupported(std::vector<vk::ExtensionProperties> & extensions, char const * name)
{
    std::vector<vk::ExtensionProperties>::const_iterator i = std::find_if(extensions.begin(),
                                                                          extensions.end(),
                                                                          [name] (vk::ExtensionProperties const & e) {
                                                                              return strcmp(name, e.extensionName) == 0;
                                                                          });

    return i != extensions.end();
}


//! @param 	device
//! @param 	extensions
bool allExtensionsSupported(vk::PhysicalDevice device, std::vector<char const *> const & extensions)
{
    std::vector<vk::ExtensionProperties> available = device.enumerateDeviceExtensionProperties(nullptr);
    for (auto const & required : extensions)
    {
        if (!Vkx::extensionIsSupported(available, required))
            return false;
    }
    return true;
}

//! @param 	layers
//! @param 	name
bool layerIsAvailable(std::vector<vk::LayerProperties> layers, char const * name)
{
    std::vector<vk::LayerProperties>::const_iterator i = std::find_if(layers.begin(),
                                                                      layers.end(),
                                                                      [name] (vk::LayerProperties const & e) {
                                                                          return strcmp(name, e.layerName) == 0;
                                                                      });

    return i != layers.end();
}

//!
//! @param 	requested
bool allLayersAvailable(std::vector<char const *> const & requested)
{
    std::vector<vk::LayerProperties> available = vk::enumerateInstanceLayerProperties();
    for (auto const & request : requested)
    {
        if (!Vkx::layerIsAvailable(available, request))
            return false;
    }
    return true;
}

//! @param 	path        Path to shader file
//! @param 	device      Logical device managing the shader
//! @param 	flags       Creation flags (default: none)
//!
//! @return shader module handle
//!
//! @warn   std::runtime_error is thrown if the file cannot be opened
vk::ShaderModule loadShaderModule(std::string const &         path,
                                  vk::Device &                device,
                                  vk::ShaderModuleCreateFlags flags /*= vk::ShaderModuleCreateFlags()*/)
{
    // Load the code
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Vkx::loadShaderModule: failed to open file");
    size_t fileSize   = (size_t)file.tellg();
    size_t shaderSize = (fileSize + (sizeof(uint32_t) - 1)) / sizeof(uint32_t);
    std::vector<uint32_t> buffer(shaderSize);
    file.seekg(0);
    file.read((char *)buffer.data(), fileSize);

    // Create the module for the device
    vk::ShaderModule shaderModule = device.createShaderModule({ flags, buffer.size() * sizeof(uint32_t), buffer.data() });

    return shaderModule;
}
} // namespace Vkx
