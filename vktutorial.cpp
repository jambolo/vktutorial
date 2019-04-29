#define GLFW_INCLUDE_VULKAN
#include <Glfwx/Glfwx.h>
#include <Vkx/Buffer.h>
#include <Vkx/Camera.h>
#include <Vkx/Image.h>
#include <Vkx/Instance.h>
#include <Vkx/SwapChain.h>
#include <Vkx/Vkx.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include "tiny_obj_loader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef NDEBUG
static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif

// These are the validation layers we will be using.
std::vector<char const *> const VALIDATION_LAYERS =
{
    "VK_LAYER_LUNARG_standard_validation"
};

// In order to display the output, we need a swap chain.
std::vector<char const *> const DEVICE_EXTENSIONS =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// This is the vertex format.
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    // Returns the create info for this vertex format (assumes one binding)
    static vk::PipelineVertexInputStateCreateInfo vertexInputInfo()
    {
        return vk::PipelineVertexInputStateCreateInfo({},
                                                      1,
                                                      &bindingDescription_,
                                                      (uint32_t)attributeDescriptions_.size(),
                                                      attributeDescriptions_.data()
        );
    }

    bool operator ==(Vertex const & rhs) const
    {
        return pos == rhs.pos && color == rhs.color && texCoord == rhs.texCoord;
    }

private:
    static vk::VertexInputBindingDescription bindingDescription_;
    static std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions_;
};

namespace std
{
template <> struct hash<Vertex>
{
    size_t operator ()(Vertex const & vertex) const
    {
        size_t h = 0;
        glm::detail::hash_combine(h, hash<glm::vec3>()(vertex.pos));
        glm::detail::hash_combine(h, hash<glm::vec3>()(vertex.color));
        glm::detail::hash_combine(h, hash<glm::vec2>()(vertex.texCoord));
        return h;
    }
};
}

// This is the layout of the vertices, basically initial offset and stride
vk::VertexInputBindingDescription Vertex::bindingDescription_ =
{
    0, sizeof(Vertex), vk::VertexInputRate::eVertex
};

// This describes the format, index, and positions of the vertex attributes, one entry for each attribute
std::array<vk::VertexInputAttributeDescription, 3> Vertex::attributeDescriptions_ =
{
    {
        { 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) },
        { 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
        { 2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord) }
    }
};

char constexpr MODEL_PATH[]   = "models/chalet.obj";
char constexpr TEXTURE_PATH[] = "textures/chalet.jpg";

struct SwapChainSupportInfo
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

bool findQueueFamilies(vk::PhysicalDevice const & physicalDevice,
                       vk::SurfaceKHR const &     surface,
                       uint32_t &                 graphicsFamily,
                       uint32_t &                 presentFamily)
{
    bool graphicsFamilyFound = false;
    bool presentFamilyFound  = false;

    std::vector<vk::QueueFamilyProperties> families = physicalDevice.getQueueFamilyProperties();
    int index = 0;
    for (auto const & f : families)
    {
        if (f.queueCount > 0)
        {
            if (f.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                graphicsFamily      = index;
                graphicsFamilyFound = true;
            }
            if (physicalDevice.getSurfaceSupportKHR(index, surface))
            {
                presentFamily      = index;
                presentFamilyFound = true;
            }
        }

        if (graphicsFamilyFound && presentFamilyFound)
            break;
        ++index;
    }
    return graphicsFamilyFound && presentFamilyFound;
}

SwapChainSupportInfo querySwapChainSupport(vk::PhysicalDevice const & physicalDevice, vk::SurfaceKHR const & surface)
{
    return
        {
            physicalDevice.getSurfaceCapabilitiesKHR(surface),
            physicalDevice.getSurfaceFormatsKHR(surface),
            physicalDevice.getSurfacePresentModesKHR(surface)
        };
}

bool isSuitable(vk::PhysicalDevice const & physicalDevice, vk::SurfaceKHR const & surface)
{
    // A suitable device has the necessary queues, device extensions, and swapchain support
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    if (!findQueueFamilies(physicalDevice, surface, graphicsFamily, presentFamily))
        return false;
    bool extensionsSupported = Vkx::allExtensionsSupported(physicalDevice, DEVICE_EXTENSIONS);
    bool swapChainAdequate   = false;
    if (extensionsSupported)
    {
        SwapChainSupportInfo swapChainSupport = querySwapChainSupport(physicalDevice, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();

    return extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

vk::SampleCountFlagBits getMaxMsaa(vk::PhysicalDeviceProperties const & properties)
{
    unsigned counts = std::min((unsigned)properties.limits.framebufferColorSampleCounts,
                               (unsigned)properties.limits.framebufferDepthSampleCounts);
    if (counts == 0)
        return vk::SampleCountFlagBits::e1;

    int best = -1;
    while (counts > 0)
    {
        ++best;
        counts >>= 1;
    }

    return vk::SampleCountFlagBits(1 << best);
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const & available)
{
    // We get to choose ...
    if (available.size() == 1 && available[0].format == vk::Format::eUndefined)
        return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

    // Has what we want?
    for (auto const & format : available)
    {
        if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    }

    // Whatever ...
    return available[0];
}

vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const & available)
{
    // Normally VK_PRESENT_MODE_FIFO_KHR is preferred over VK_PRESENT_MODE_IMMEDIATE_KHR, but ...
    //      "Unfortunately some drivers currently don't properly support VK_PRESENT_MODE_FIFO_KHR, so we should prefer
    //      VK_PRESENT_MODE_IMMEDIATE_KHR if VK_PRESENT_MODE_MAILBOX_KHR is not available."

    vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

    for (auto const & mode : available)
    {
        if (mode == vk::PresentModeKHR::eMailbox)
            return mode;
        else if (mode == vk::PresentModeKHR::eImmediate)
            bestMode = mode;
    }

    return bestMode;
}

vk::Extent2D chooseSwapExtent(Glfwx::Window const & window, vk::SurfaceCapabilitiesKHR const & capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        window.framebufferSize(width, height);
        return { (uint32_t)width, (uint32_t)height };
    }
}

vk::Format findSupportedFormat(vk::PhysicalDevice const &      physicalDevice,
                               std::vector<vk::Format> const & candidates,
                               vk::ImageTiling                 tiling,
                               vk::FormatFeatureFlags          features)
{
    for (auto format : candidates)
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
            return format;
    }
    throw std::runtime_error("findSupportedFormat: failed to find supported format");
}

vk::Format findDepthFormat(vk::PhysicalDevice const & physicalDevice)
{
    return findSupportedFormat(physicalDevice,
                               { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
                               vk::ImageTiling::eOptimal,
                               vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

class HelloTriangleApplication
{
public:
    void run()
    {
        initializeWindow();
        initializeVulkan();

        // Create a display surface. This is system-dependent feature and we use glfw to handle that.
        surface_ = vk::UniqueSurfaceKHR(window_->createSurface(*instance_, nullptr), *instance_);

        choosePhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createCommandPools();
        createColorResources();
        createDepthResources();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createTextureImage();
        createTextureSampler();
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();

        Vkx::Camera camera(glm::radians(90.0f),
                           0.1f,
                           10.0f,
                           (float)swapChain_->extent().width / (float)swapChain_->extent().height);
        camera.lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        while (!window_->processEvents())
        {
            drawFrame(camera);
        }
        device_->waitIdle();
    }

private:

    static int constexpr WIDTH  = 1920;
    static int constexpr HEIGHT = 1440;
    static int constexpr MAX_FRAMES_IN_FLIGHT = 2;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
    };

    void initializeWindow()
    {
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        window_ = std::make_unique<Glfwx::Window>(WIDTH, HEIGHT, "vktutorial");
        window_->setFramebufferSizeCallback(
            [this] (Glfwx::Window *, int, int)
            {
                this->framebufferSizeChanged_ = true;
            });
    }

    void initializeVulkan()
    {
        // If we want validation layers, we must check that they are available
        if (VALIDATION_LAYERS_REQUESTED && !Vkx::allLayersAvailable(VALIDATION_LAYERS))
            throw std::runtime_error("validation layers requested, but not available!");

        vk::ApplicationInfo appInfo("Hello Triangle",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    "No Engine",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    VK_API_VERSION_1_0);
        // Some extensions are necessary.
        auto requiredExtensions = myRequiredExtensions();

        vk::InstanceCreateInfo createInfo({},
                                          &appInfo,
                                          0,
                                          nullptr,
                                          (uint32_t)requiredExtensions.size(),
                                          requiredExtensions.data());
        if (VALIDATION_LAYERS_REQUESTED)
        {
            createInfo.setEnabledLayerCount((uint32_t)VALIDATION_LAYERS.size());
            createInfo.setPpEnabledLayerNames(VALIDATION_LAYERS.data());
        }

        // Create a Vulkan instance
        instance_ = std::make_shared<Vkx::Instance>(createInfo);

        // Some objects, e.g. the validation layer, are not linked to the static loader and must be loaded by a dynamic loader.
        dynamicLoader_.init(*instance_, vkGetInstanceProcAddr);

        // Set up validation callbacks
        setupDebugMessenger();
    }

    // Some extensions are required. We collect the names here
    std::vector<char const *> myRequiredExtensions()
    {
        std::vector<char const *> requiredExtensions = Glfwx::requiredInstanceExtensions();

        if (VALIDATION_LAYERS_REQUESTED)
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return requiredExtensions;
    }

    // Link up to the debug messenger extension, which will feed us debugging output from the validation layers.
    void setupDebugMessenger()
    {
        if (!VALIDATION_LAYERS_REQUESTED)
            return;

        messenger_ = instance_->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT({},
                                                 (vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose),
                                                 Vkx::DebugUtils::MessageTypeFlags::all,
                                                 debugCallback),
            nullptr,
            dynamicLoader_);
    }

    // This callback is called by the debug messenger extension to handle the debug output from the validation layers.
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
                                                        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
                                                        void *                                       pUserData)
    {
        std::cerr << "(" << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ")"
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << " - "
                  << pCallbackData->pMessage
                  << std::endl;
        return VK_FALSE;
    }

    void choosePhysicalDevice()
    {
        // Find a physical device that has the appropriate functionality for what we need.
        physicalDevice_ =
            std::make_shared<Vkx::PhysicalDevice>(instance_,
                                                  *surface_,
                                                  [this] (std::vector<vk::PhysicalDevice> const & physicalDevices) {
                                                      // Basically, we just pick the first one that is suitable.
                                                      for (auto const & physicalDevice : physicalDevices)
                                                      {
                                                          if (isSuitable(physicalDevice, *surface_))
                                                              return physicalDevice;
                                                      }

                                                      throw std::runtime_error(
                                                          "choosePhysicalDevice: failed to find a suitable GPU!");
                                                  });
        vk::PhysicalDeviceProperties properties = physicalDevice_->getProperties();
        msaa_ = getMaxMsaa(properties);
#if 0
        {
            std::cerr << "Physical Device chosen: " << properties.deviceName
                      << ", version: " << properties.driverVersion
                      << std::endl;

            vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice_.getMemoryProperties();

            std::cerr << "    Memory types:" << std::endl;
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
            {
                std::cerr << "        "
                          << i
                          << ": properties - " << vk::to_string(memoryProperties.memoryTypes[i].propertyFlags)
                          << ", heapIndex - " << memoryProperties.memoryTypes[i].heapIndex
                          << std::endl;
            }

            std::cerr << "    Heaps:" << std::endl;
            for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
            {
                std::cerr << "        "
                          << i
                          << ": size - " << double(memoryProperties.memoryHeaps[i].size) / double(1024 * 1024 * 1024)
                          << " GiB, flags - " << vk::to_string(memoryProperties.memoryHeaps[i].flags)
                          << std::endl;
            }
        }
#endif  // if 0
    }

    void createLogicalDevice()
    {
        findQueueFamilies(*physicalDevice_, physicalDevice_->surface(), graphicsFamily_, presentFamily_);

        float priority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), graphicsFamily_, 1, &priority);
        if (graphicsFamily_ != presentFamily_)
            queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), presentFamily_, 1, &priority);

        vk::PhysicalDeviceFeatures deviceFeatures;
        deviceFeatures.setSamplerAnisotropy(VK_TRUE);

        vk::DeviceCreateInfo createInfo({},
                                        (uint32_t)queueCreateInfos.size(),
                                        queueCreateInfos.data(),
                                        0,
                                        nullptr,
                                        (int)DEVICE_EXTENSIONS.size(),
                                        DEVICE_EXTENSIONS.data(),
                                        &deviceFeatures);
        if (VALIDATION_LAYERS_REQUESTED)
        {
            createInfo.setPpEnabledLayerNames(VALIDATION_LAYERS.data());
            createInfo.setEnabledLayerCount(static_cast<uint32_t>(VALIDATION_LAYERS.size()));
        }

        device_        = std::make_shared<Vkx::Device>(physicalDevice_, createInfo);
        graphicsQueue_ = device_->getQueue(graphicsFamily_, 0);
        presentQueue_  = device_->getQueue(presentFamily_, 0);
    }

    void createSwapChain()
    {
        std::shared_ptr<Vkx::PhysicalDevice> physicalDevice = device_->physical();
        vk::SurfaceKHR       surface          = physicalDevice->surface();
        SwapChainSupportInfo swapChainSupport = querySwapChainSupport(*physicalDevice, surface);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D         extent        = chooseSwapExtent(*window_, swapChainSupport.capabilities);

        swapChain_ = std::make_shared<Vkx::SwapChain>(device_,
                                                      surfaceFormat,
                                                      extent,
                                                      graphicsFamily_,
                                                      presentFamily_,
                                                      presentMode);
        framebufferSizeChanged_ = false;
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment({},
                                                  swapChain_->format(),
                                                  msaa_,
                                                  vk::AttachmentLoadOp::eClear,
                                                  vk::AttachmentStoreOp::eStore,
                                                  vk::AttachmentLoadOp::eDontCare,
                                                  vk::AttachmentStoreOp::eDontCare,
                                                  vk::ImageLayout::eUndefined,
                                                  vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::AttachmentDescription depthAttachment({},
                                                  depthImage_.info().format,
                                                  msaa_,
                                                  vk::AttachmentLoadOp::eClear,
                                                  vk::AttachmentStoreOp::eDontCare,
                                                  vk::AttachmentLoadOp::eDontCare,
                                                  vk::AttachmentStoreOp::eDontCare,
                                                  vk::ImageLayout::eUndefined,
                                                  vk::ImageLayout::eDepthStencilAttachmentOptimal);
        vk::AttachmentReference depthAttachmentRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentDescription resolveAttachment({},
                                                    swapChain_->format(),
                                                    vk::SampleCountFlagBits::e1,
                                                    vk::AttachmentLoadOp::eDontCare,
                                                    vk::AttachmentStoreOp::eStore,
                                                    vk::AttachmentLoadOp::eDontCare,
                                                    vk::AttachmentStoreOp::eDontCare,
                                                    vk::ImageLayout::eUndefined,
                                                    vk::ImageLayout::ePresentSrcKHR);
        vk::AttachmentReference resolveAttachmentRef(2, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass({},
                                       vk::PipelineBindPoint::eGraphics,
                                       0,
                                       nullptr,
                                       1,
                                       &colorAttachmentRef,
                                       &resolveAttachmentRef,
                                       &depthAttachmentRef);

        vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL,
                                         0,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::AccessFlags(),
                                         vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

        std::array<vk::AttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, resolveAttachment };
        renderPass_ = device_->createRenderPassUnique(
            vk::RenderPassCreateInfo({},
                                     (uint32_t)attachments.size(),
                                     attachments.data(),
                                     1,
                                     &subpass,
                                     1,
                                     &dependency));
    }

    void createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding bindings[] =
        {
            vk::DescriptorSetLayoutBinding(0,
                                           vk::DescriptorType::eUniformBuffer,
                                           1,
                                           vk::ShaderStageFlagBits::eVertex),
            vk::DescriptorSetLayoutBinding(1,
                                           vk::DescriptorType::eCombinedImageSampler,
                                           1,
                                           vk::ShaderStageFlagBits::eFragment)
        };

        descriptorSetLayout_ = device_->createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo({}, 2, bindings));
    }

    void createGraphicsPipeline()
    {
        vk::UniqueShaderModule vertShaderModule(Vkx::loadShaderModule("shaders/shader.vert.spv", device_), *device_);
        vk::UniqueShaderModule fragShaderModule(Vkx::loadShaderModule("shaders/shader.frag.spv", device_), *device_);

        vk::PipelineShaderStageCreateInfo shaderStages[] =
        {
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"),
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main")
        };

        vk::PipelineVertexInputStateCreateInfo   vertexInputInfo = Vertex::vertexInputInfo();
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

        vk::Viewport viewport(0.0f, 0.0f, (float)swapChain_->extent().width, (float)swapChain_->extent().height, 0.0f, 1.0f);
        vk::Rect2D   scissor({ 0, 0 }, swapChain_->extent());
        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
        rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);  // This is bullshit because of glm::lookAt
        rasterizer.setLineWidth(1.0);

        vk::PipelineMultisampleStateCreateInfo multisampling({}, msaa_);

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.setColorWriteMask(Vkx::ColorComponentFlags::all);

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.setPAttachments(&colorBlendAttachment);
        colorBlending.setAttachmentCount(1);

        pipelineLayout_ = device_->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, 1, &descriptorSetLayout_.get()));

        vk::PipelineDepthStencilStateCreateInfo depthStencil({},
                                                             VK_TRUE,
                                                             VK_TRUE,
                                                             vk::CompareOp::eLess,
                                                             VK_FALSE,
                                                             VK_FALSE);
        graphicsPipeline_ = device_->createGraphicsPipelineUnique(
            vk::PipelineCache(),
            vk::GraphicsPipelineCreateInfo({},
                                           2,
                                           shaderStages,
                                           &vertexInputInfo,
                                           &inputAssembly,
                                           nullptr,
                                           &viewportState,
                                           &rasterizer,
                                           &multisampling,
                                           &depthStencil,
                                           &colorBlending,
                                           nullptr,
                                           *pipelineLayout_,
                                           *renderPass_,
                                           0));
    }

    void createCommandPools()
    {
        graphicsCommandPool_  = device_->createCommandPoolUnique(vk::CommandPoolCreateInfo({}, graphicsFamily_));
        transientCommandPool_ = device_->createCommandPoolUnique(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient,
                                      graphicsFamily_));
    }

    void createColorResources()
    {
        resolveImage_ = Vkx::ResolveImage(device_,
                                          transientCommandPool_.get(),
                                          graphicsQueue_,
                                          vk::ImageCreateInfo({},
                                                              vk::ImageType::e2D,
                                                              swapChain_->format(),
                                                              { swapChain_->extent().width, swapChain_->extent().height, 1 },
                                                              1,
                                                              1,
                                                              msaa_,
                                                              vk::ImageTiling::eOptimal,
                                                              vk::ImageUsageFlagBits::eTransientAttachment |
                                                              vk::ImageUsageFlagBits::eColorAttachment));
    }

    void createDepthResources()
    {
        vk::Format format = findDepthFormat(*device_->physical());
        depthImage_ = Vkx::DepthImage(device_,
                                      transientCommandPool_.get(),
                                      graphicsQueue_,
                                      vk::ImageCreateInfo({},
                                                          vk::ImageType::e2D,
                                                          format,
                                                          { swapChain_->extent().width, swapChain_->extent().height, 1 },
                                                          1,
                                                          1,
                                                          msaa_,
                                                          vk::ImageTiling::eOptimal,
                                                          vk::ImageUsageFlagBits::eDepthStencilAttachment));
    }

    void createFramebuffers()
    {
        size_t count = swapChain_->size();
        framebuffers_.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            std::array<vk::ImageView, 3> attachments = { resolveImage_.view(), depthImage_.view(), swapChain_->view(i) };
            framebuffers_.push_back(
                device_->createFramebufferUnique(
                    vk::FramebufferCreateInfo({},
                                              *renderPass_,
                                              (uint32_t)attachments.size(),
                                              attachments.data(),
                                              swapChain_->extent().width,
                                              swapChain_->extent().height,
                                              1)));
        }
    }

    void createTextureImage()
    {
        int       width, height, channels;
        stbi_uc * pixels = stbi_load(TEXTURE_PATH, &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("createTextureImage: failed to load texture image!");
        VkDeviceSize imageSize = width * height * 4;
        uint32_t     mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
        textureImage_ = Vkx::LocalImage(device_,
                                        transientCommandPool_.get(),
                                        graphicsQueue_,
                                        vk::ImageCreateInfo({},
                                                            vk::ImageType::e2D,
                                                            vk::Format::eR8G8B8A8Unorm,
                                                            { (uint32_t)width, (uint32_t)height, 1 },
                                                            mipLevels,
                                                            1,
                                                            vk::SampleCountFlagBits::e1,
                                                            vk::ImageTiling::eOptimal,
                                                            vk::ImageUsageFlagBits::eTransferSrc |
                                                            vk::ImageUsageFlagBits::eTransferDst |
                                                            vk::ImageUsageFlagBits::eSampled),
                                        pixels,
                                        imageSize);
        stbi_image_free(pixels);
    }

    void createTextureSampler()
    {
        textureSampler_ = device_->createSamplerUnique(
            vk::SamplerCreateInfo({},
                                  vk::Filter::eLinear,
                                  vk::Filter::eLinear,
                                  vk::SamplerMipmapMode::eLinear,
                                  vk::SamplerAddressMode::eRepeat,
                                  vk::SamplerAddressMode::eRepeat,
                                  vk::SamplerAddressMode::eRepeat,
                                  0.0f,
                                  VK_TRUE,
                                  16,
                                  VK_FALSE,
                                  vk::CompareOp::eAlways,
                                  0.0f,
                                  (float)textureImage_.info().mipLevels,
                                  vk::BorderColor::eIntOpaqueBlack,
                                  VK_FALSE));
    }

    void loadModel()
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH))
            throw std::runtime_error(warn + err);

        std::unordered_map<Vertex, uint32_t> uniqueVertices;
        for (auto const & shape : shapes)
        {
            for (auto const & index : shape.mesh.indices)
            {
                Vertex vertex;
                vertex.pos =
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = { 1.0f, 1.0f, 1.0f };

                uint32_t uniqueIndex;
                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueIndex = (uint32_t)vertices_.size();
                    vertices_.push_back(vertex);
                    uniqueVertices[vertex] = uniqueIndex;
                }
                else
                {
                    uniqueIndex = uniqueVertices[vertex];
                }

                indices_.push_back(uniqueIndex);
            }
        }
    }

    void createVertexBuffer()
    {
        vertexBuffer_ = Vkx::LocalBuffer(device_,
                                         transientCommandPool_.get(),
                                         graphicsQueue_,
                                         vertices_.size() * sizeof(vertices_[0]),
                                         vk::BufferUsageFlagBits::eVertexBuffer,
                                         vertices_.data());
    }

    void createIndexBuffer()
    {
        indexBuffer_ = Vkx::LocalBuffer(device_,
                                        transientCommandPool_.get(),
                                        graphicsQueue_,
                                        indices_.size() * sizeof(indices_[0]),
                                        vk::BufferUsageFlagBits::eIndexBuffer,
                                        indices_.data());
    }

    void createUniformBuffers()
    {
        size_t size = sizeof(UniformBufferObject);
        uniformBuffers_.reserve(swapChain_->size());

        for (size_t i = 0; i < swapChain_->size(); ++i)
        {
            uniformBuffers_.emplace_back(device_, size, vk::BufferUsageFlagBits::eUniformBuffer);
        }
    }

    void createDescriptorPool()
    {
        vk::DescriptorPoolSize poolSizes[] =
        {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, (uint32_t)swapChain_->size()),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, (uint32_t)swapChain_->size())
        };
        descriptorPool_ = device_->createDescriptorPoolUnique(
            vk::DescriptorPoolCreateInfo({}, (uint32_t)swapChain_->size(), 2, poolSizes));
    }

    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(swapChain_->size(), descriptorSetLayout_.get());
        descriptorSets_ = device_->allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo(descriptorPool_.get(), (uint32_t)layouts.size(), layouts.data()));
        for (size_t i = 0; i < swapChain_->size(); ++i)
        {
            vk::DescriptorBufferInfo uboInfo(uniformBuffers_[i], 0, sizeof(UniformBufferObject));
            vk::DescriptorImageInfo  imageInfo(textureSampler_.get(),
                                               textureImage_.view(),
                                               vk::ImageLayout::eShaderReadOnlyOptimal);
            std::array<vk::WriteDescriptorSet, 2> writeDescriptorSets =
            {
                vk::WriteDescriptorSet(descriptorSets_[i],
                                       0,
                                       0,
                                       1,
                                       vk::DescriptorType::eUniformBuffer,
                                       nullptr,
                                       &uboInfo,
                                       nullptr),
                vk::WriteDescriptorSet(descriptorSets_[i],
                                       1,
                                       0,
                                       1,
                                       vk::DescriptorType::eCombinedImageSampler,
                                       &imageInfo,
                                       nullptr,
                                       nullptr),
            };
            device_->updateDescriptorSets((uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
        }
    }

    void createCommandBuffers()
    {
        commandBuffers_ = device_->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo(*graphicsCommandPool_,
                                          vk::CommandBufferLevel::ePrimary,
                                          (uint32_t)swapChain_->size()));

        vk::Buffer     vertexBuffers[] = { vertexBuffer_ };
        vk::DeviceSize offsets[]       = { 0 };
        std::array<vk::ClearValue, 2> clearValues =
        {
            vk::ClearColorValue(std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f }),
            vk::ClearDepthStencilValue(1.0f, 0)
        };

        int i = 0;
        for (auto & buffer : commandBuffers_)
        {
            buffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
            buffer->beginRenderPass(
                vk::RenderPassBeginInfo(*renderPass_,
                                        *framebuffers_[i],
                                        {{ 0, 0 }, swapChain_->extent() },
                                        (uint32_t)clearValues.size(),
                                        clearValues.data()),
                vk::SubpassContents::eInline);
            buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
            buffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
            buffer->bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint32);
            buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       pipelineLayout_.get(), 0, 1, &descriptorSets_[i], 0, nullptr);
            buffer->drawIndexed((uint32_t)indices_.size(), 1, 0, 0, 0);
            buffer->endRenderPass();
            buffer->end();
            ++i;
        }
    }

    void drawFrame(Vkx::Camera const & camera)
    {
        uint32_t swapIndex;
        try
        {
            swapIndex = swapChain_->swap();
        }
        catch (vk::OutOfDateKHRError &)
        {
            recreateSwapChain();
            return;
        }

        updateUniformBuffer(camera, swapIndex);

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo         submitInfo(1,
                                          &swapChain_->imageAvailable(),
                                          &waitStage,
                                          1,
                                          &(*commandBuffers_[swapIndex]),
                                          1,
                                          &swapChain_->renderFinished());
        graphicsQueue_.submit(1, &submitInfo, swapChain_->inFlight());

        try
        {
            std::array<vk::SwapchainKHR, 1> swapChains = { *swapChain_ };
            vk::Result result = presentQueue_.presentKHR(
                vk::PresentInfoKHR(1,
                                   &swapChain_->renderFinished(),
                                   (uint32_t)swapChains.size(),
                                   swapChains.data(),
                                   &swapIndex));
            if (result == vk::Result::eSuboptimalKHR || framebufferSizeChanged_)
                recreateSwapChain();
        }
        catch (vk::OutOfDateKHRError &)
        {
            recreateSwapChain();
        }
    }

    void updateUniformBuffer(Vkx::Camera const & camera, int index)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo;
        ubo.model      = glm::rotate(glm::mat4(1.0f), time * glm::pi<float>() / 8.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view       = camera.view();
        ubo.projection = camera.projection();

        uniformBuffers_[index].set(0, &ubo, sizeof(ubo));
    }

    void resetSwapChain()
    {
        framebuffers_.clear();
        commandBuffers_.clear();
        graphicsPipeline_.reset();
        pipelineLayout_.reset();
        renderPass_.reset();
        swapChain_.reset();
    }

    void recreateSwapChain()
    {
        int width, height;
        window_->framebufferSize(width, height);
        while (width == 0 || height == 0)
        {
            window_->waitEvents();
            window_->framebufferSize(width, height);
        }

        vkDeviceWaitIdle(*device_);

        resetSwapChain();

        createSwapChain();
        createRenderPass();
        createGraphicsPipeline();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        createCommandBuffers();
    }

    std::unique_ptr<Glfwx::Window> window_ = nullptr;
    std::shared_ptr<Vkx::Instance> instance_;
    vk::DispatchLoaderDynamic dynamicLoader_;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> messenger_;
    uint32_t graphicsFamily_;
    uint32_t presentFamily_;
    vk::SampleCountFlagBits msaa_ = vk::SampleCountFlagBits::e1;
    std::shared_ptr<Vkx::PhysicalDevice> physicalDevice_;
    std::shared_ptr<Vkx::Device> device_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    vk::UniqueSurfaceKHR surface_;
    std::shared_ptr<Vkx::SwapChain> swapChain_;
    vk::UniqueRenderPass renderPass_;
    vk::UniqueDescriptorSetLayout descriptorSetLayout_;
    vk::UniquePipelineLayout pipelineLayout_;
    vk::UniquePipeline graphicsPipeline_;
    std::vector<vk::UniqueFramebuffer> framebuffers_;
    vk::UniqueCommandPool graphicsCommandPool_;
    vk::UniqueCommandPool transientCommandPool_;
    Vkx::ResolveImage resolveImage_;
    Vkx::DepthImage depthImage_;
    Vkx::LocalImage textureImage_;
    vk::UniqueSampler textureSampler_;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    Vkx::LocalBuffer vertexBuffer_;
    Vkx::LocalBuffer indexBuffer_;
    std::vector<Vkx::HostBuffer> uniformBuffers_;
    vk::UniqueDescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;
    std::vector<vk::UniqueCommandBuffer> commandBuffers_;
    bool framebufferSizeChanged_ = false;
};

int main()
{
    Glfwx::Instance glfwx;
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
