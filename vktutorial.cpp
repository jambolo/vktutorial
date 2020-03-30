#define GLFW_INCLUDE_VULKAN
#include <Glfwx/Glfwx.h>
#include <Vkx/Buffer.h>
#include <Vkx/Camera.h>
#include <Vkx/Image.h>
#include <Vkx/Instance.h>
#include <Vkx/Model.h>
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

#define MODEL_IS_DRAWN 1

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

#if defined(MODEL_IS_DRAWN)
char constexpr MODEL_PATH[]           = "models/chalet.obj";
char constexpr TEXTURE_PATH[]         = "textures/chalet.jpg";
char constexpr VERTEX_SHADER_PATH[]   = "shaders/shader.vert.spv";
char constexpr FRAGMENT_SHADER_PATH[] = "shaders/shader.frag.spv";
#endif

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
        createFramebuffers();
#if defined(MODEL_IS_DRAWN)
        createModel();
#endif  // defined(MODEL_IS_DRAWN)
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
        window_->setFramebufferSizeChangedCallback(
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

    void createModel()
    {
        model_ = std::make_shared<Vkx::Model>(MODEL_PATH,
                                              TEXTURE_PATH,
                                              VERTEX_SHADER_PATH,
                                              FRAGMENT_SHADER_PATH,
                                              swapChain_->size(),
                                              swapChain_->extent(),
                                              msaaSamples_);
    }

    void createCommandBuffers()
    {
        commandBuffers_ = device_->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo(*graphicsCommandPool_,
                                          vk::CommandBufferLevel::ePrimary,
                                          (uint32_t)swapChain_->size()));

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
            model_->draw(*buffer);
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
        model_->reset();
        commandBuffers_.clear();
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
        model_->recreate();
        createColorResources();
        createDepthResources();
        createFramebuffers();
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
    std::vector<vk::UniqueFramebuffer> framebuffers_;
    vk::UniqueCommandPool graphicsCommandPool_;
    vk::UniqueCommandPool transientCommandPool_;
    Vkx::ResolveImage resolveImage_;
    Vkx::DepthImage depthImage_;
#if defined(MODEL_IS_DRAWN)
    std::shared_ptr<Vkx::Model> model_;
#endif // defined(MODEL_IS_DRAWN)
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
