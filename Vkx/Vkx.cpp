#include "Vkx.h"

#include "Buffer.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace Vkx
{
//! @param  extensions
//! @param  name
bool extensionIsSupported(std::vector<vk::ExtensionProperties> & extensions, char const * name)
{
    std::vector<vk::ExtensionProperties>::const_iterator i = std::find_if(extensions.begin(),
                                                                          extensions.end(),
                                                                          [name] (vk::ExtensionProperties const & e) {
                                                                              return strcmp(name, e.extensionName) == 0;
                                                                          });

    return i != extensions.end();
}

//! @param  device
//! @param  extensions
bool allExtensionsSupported(vk::PhysicalDevice device, std::vector<char const *> const & extensions)
{
    std::vector<vk::ExtensionProperties> available = device.enumerateDeviceExtensionProperties(nullptr);
    for (auto const & required : extensions)
    {
        if (!extensionIsSupported(available, required))
            return false;
    }
    return true;
}

//! @param  layers
//! @param  name
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
//! @param  requested
bool allLayersAvailable(std::vector<char const *> const & requested)
{
    std::vector<vk::LayerProperties> available = vk::enumerateInstanceLayerProperties();
    for (auto const & request : requested)
    {
        if (!layerIsAvailable(available, request))
            return false;
    }
    return true;
}

//! @param  path        Path to shader file
//! @param  device      Logical device managing the shader
//! @param  flags       Creation flags (default: none)
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

//! @param  physicalDevice      The physical device that will allocate the memory
//! @param  types               Acceptable memory types as determined by vk::Device::getBufferMemoryRequirements()
//! @param  properties          Necessary properties
//!
//! @return     index of the type of memory provided by the physical device that matches the request
//!
//! @warning    A std::runtime_error is thrown if an appropriate type is not available
uint32_t findAppropriateMemoryType(vk::PhysicalDevice physicalDevice, uint32_t types, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties info = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < info.memoryTypeCount; ++i)
    {
        if ((types & (1 << i)) == 0)
            continue;
        if ((info.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Vkx::findAppropriateMemoryType: failed to find appropriate memory type");
}

//! @param  device
//! @param  src
//! @param  dst
//! @param  size
//! @param  commandPool
//! @param  queue
void copyBufferSynced(vk::Device      device,
                      vk::Buffer      src,
                      vk::Buffer      dst,
                      vk::DeviceSize  size,
                      vk::CommandPool commandPool,
                      vk::Queue       queue)
{
    std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo(commandPool,
                                      vk::CommandBufferLevel::ePrimary,
                                      1));
    commandBuffers[0].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    commandBuffers[0].copyBuffer(src, dst, vk::BufferCopy(0, 0, size));
    commandBuffers[0].end();
    queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &commandBuffers[0]), nullptr);
    vkQueueWaitIdle(queue);
    device.free(commandPool, 1, &commandBuffers[0]);
}

Buffer createDeviceLocalBuffer(vk::Device           device,
                               vk::PhysicalDevice   physicalDevice,
                               vk::BufferUsageFlags flags,
                               void const *         src,
                               vk::DeviceSize       size,
                               vk::CommandPool      commandPool,
                               vk::Queue            queue)
{
    Buffer staging(device,
                   physicalDevice,
                   size,
                   vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void * data = device.mapMemory(staging.allocation(), 0, size, vk::MemoryMapFlags());
    memcpy(data, src, size);
    device.unmapMemory(staging.allocation());

    Buffer buffer(device,
                  physicalDevice,
                  size,
                  flags | vk::BufferUsageFlagBits::eTransferDst,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBufferSynced(device, staging, buffer, size, commandPool, queue);

    return buffer;
}
} // namespace Vkx
