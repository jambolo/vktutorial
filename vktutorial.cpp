#define GLFW_INCLUDE_VULKAN
#include "Glfwx/Glfwx.h"
#include "Vkx/Buffer.h"
#include "Vkx/Vkx.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
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
    glm::vec2 pos;
    glm::vec3 color;

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

private:
    static vk::VertexInputBindingDescription bindingDescription_;
    static std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions_;
};

// This is the layout of the vertices, basically initial offset and stride
vk::VertexInputBindingDescription Vertex::bindingDescription_ =
{
    0, sizeof(Vertex), vk::VertexInputRate::eVertex
};

// This describes the format, index, and positions of the vertex attributes, one entry for each attribute
std::array<vk::VertexInputAttributeDescription, 2> Vertex::attributeDescriptions_ =
{
    {
        { 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) },
        { 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) }
    }
};

// This is the vertex data that is loaded into the vertex buffer. It must match the attribute descriptions.
static Vertex const vertices[] =
{
    {{ -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    {{ 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    {{ 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
    {{ -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } }
};

const uint16_t indices[] =
{
    0, 1, 2, 2, 3, 0
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initializeWindow();
        initializeVulkan();
        while (!window_->processEvents())
        {
            drawFrame();
        }
        device_->waitIdle();
    }

private:

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;
    static int constexpr MAX_FRAMES_IN_FLIGHT = 2;

    struct SwapChainSupportInfo
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
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
        instance_ = vk::createInstanceUnique(createInfo);

        // Some objects, e.g. the validation layer, are not linked to the static loader and must be loaded by a dynamic loader.
        dynamicLoader_.init(*instance_);

        // Set up validation callbacks
        setupDebugMessenger();

        // Create a display surface. This is system-dependent feature and we use glfw to handle that.
        surface_ = vk::UniqueSurfaceKHR(window_->createSurface(*instance_, nullptr), *instance_);

        choosePhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPools();
        createVertexBuffer();
        createIndexBuffer();
        createCommandBuffers();
        createSemaphores();
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
        physicalDevice_ = firstSuitablePhysicalDevice();

#if !defined(NDEBUG)
        {
            vk::PhysicalDeviceProperties properties = physicalDevice_.getProperties();

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
#endif  // if !defined(NDEBUG)
    }

    vk::PhysicalDevice firstSuitablePhysicalDevice()
    {
        // Basically, we just enumerate the physical devices and pick the first one that is suitable.
        std::vector<vk::PhysicalDevice> physicalDevices = instance_->enumeratePhysicalDevices();
        for (auto const & device : physicalDevices)
        {
            if (isSuitable(device))
                return device;
        }

        throw std::runtime_error("failed to find a suitable GPU!");
    }

    bool isSuitable(vk::PhysicalDevice const & device)
    {
        // A suitable device has the necessary queues, device extensions, and swapchain support
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        if (!findQueueFamilies(device, graphicsFamily, presentFamily))
            return false;
        bool extensionsSupported = Vkx::allExtensionsSupported(device, DEVICE_EXTENSIONS);
        bool swapChainAdequate   = false;
        if (extensionsSupported)
        {
            SwapChainSupportInfo swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        return extensionsSupported && swapChainAdequate;
    }

    SwapChainSupportInfo querySwapChainSupport(vk::PhysicalDevice const & device)
    {
        return
            {
                device.getSurfaceCapabilitiesKHR(*surface_),
                device.getSurfaceFormatsKHR(*surface_),
                device.getSurfacePresentModesKHR(*surface_)
            };
    }

    void createLogicalDevice()
    {
        findQueueFamilies(physicalDevice_, graphicsFamily_, presentFamily_);

        float priority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), graphicsFamily_, 1, &priority);
        if (graphicsFamily_ != presentFamily_)
            queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), presentFamily_, 1, &priority);

        vk::PhysicalDeviceFeatures deviceFeatures;
        vk::DeviceCreateInfo       createInfo({},
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

        device_        = physicalDevice_.createDeviceUnique(createInfo);
        graphicsQueue_ = device_->getQueue(graphicsFamily_, 0);
        presentQueue_  = device_->getQueue(presentFamily_, 0);
    }

    bool findQueueFamilies(vk::PhysicalDevice device, uint32_t & graphicsFamily, uint32_t & presentFamily)
    {
        bool graphicsFamilyFound = false;
        bool presentFamilyFound  = false;

        std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();
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
                if (device.getSurfaceSupportKHR(index, *surface_))
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

    void createSwapChain()
    {
        SwapChainSupportInfo swapChainSupport = querySwapChainSupport(physicalDevice_);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        vk::SwapchainCreateInfoKHR createInfo({},
                                              *surface_,
                                              imageCount,
                                              surfaceFormat.format,
                                              surfaceFormat.colorSpace,
                                              extent,
                                              1,
                                              vk::ImageUsageFlagBits::eColorAttachment);
        uint32_t queueFamilyIndices[] =
        {
            graphicsFamily_,
            presentFamily_
        };

        if (graphicsFamily_ != presentFamily_)
        {
            createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.presentMode  = presentMode;
        createInfo.clipped      = VK_TRUE;

        swapChain_              = device_->createSwapchainKHRUnique(createInfo);
        swapChainImages_        = device_->getSwapchainImagesKHR(*swapChain_);
        swapChainImageFormat_   = surfaceFormat.format;
        swapChainExtent_        = extent;
        framebufferSizeChanged_ = false;
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

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const & capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            window_->framebufferSize(width, height);
            return { (uint32_t)width, (uint32_t)height };
        }
    }

    void createImageViews()
    {
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        swapChainImageViews_.reserve(swapChainImages_.size());
        for (auto const & image : swapChainImages_)
        {
            swapChainImageViews_.push_back(
                device_->createImageViewUnique(
                    vk::ImageViewCreateInfo({},
                                            image,
                                            vk::ImageViewType::e2D,
                                            swapChainImageFormat_,
                                            vk::ComponentMapping(),
                                            subresourceRange)));
        }
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment({},
                                                  swapChainImageFormat_,
                                                  vk::SampleCountFlagBits::e1,
                                                  vk::AttachmentLoadOp::eClear,
                                                  vk::AttachmentStoreOp::eStore,
                                                  vk::AttachmentLoadOp::eDontCare,
                                                  vk::AttachmentStoreOp::eDontCare,
                                                  vk::ImageLayout::eUndefined,
                                                  vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::SubpassDescription  subpass({},
                                        vk::PipelineBindPoint::eGraphics,
                                        0,
                                        nullptr,
                                        1,
                                        &colorAttachmentRef);

        vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL,
                                         0,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::AccessFlags(),
                                         vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

        renderPass_ = device_->createRenderPassUnique(
            vk::RenderPassCreateInfo({},
                                     1,
                                     &colorAttachment,
                                     1,
                                     &subpass,
                                     1,
                                     &dependency));
    }

    void createGraphicsPipeline()
    {
        vk::UniqueShaderModule vertShaderModule(Vkx::loadShaderModule("shaders/shader.vert.spv", *device_), *device_);
        vk::UniqueShaderModule fragShaderModule(Vkx::loadShaderModule("shaders/shader.frag.spv", *device_), *device_);

        vk::PipelineShaderStageCreateInfo shaderStages[] =
        {
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"),
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main")
        };

        vk::PipelineVertexInputStateCreateInfo   vertexInputInfo = Vertex::vertexInputInfo();
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

        vk::Viewport viewport(0.0f, 0.0f, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0.0f, 1.0f);
        vk::Rect2D   scissor({ 0, 0 }, swapChainExtent_);
        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
        rasterizer.setFrontFace(vk::FrontFace::eClockwise);
        rasterizer.setLineWidth(1.0);

        vk::PipelineMultisampleStateCreateInfo multisampling;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.setColorWriteMask(Vkx::ColorComponentFlags::all);

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.setPAttachments(&colorBlendAttachment);
        colorBlending.setAttachmentCount(1);

        pipelineLayout_ = device_->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo());

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
                                           nullptr,
                                           &colorBlending,
                                           nullptr,
                                           *pipelineLayout_,
                                           *renderPass_,
                                           0));
    }

    void createFramebuffers()
    {
        swapChainFramebuffers_.reserve(swapChainImageViews_.size());
        for (auto const & view : swapChainImageViews_)
        {
            swapChainFramebuffers_.push_back(
                device_->createFramebufferUnique(
                    vk::FramebufferCreateInfo({},
                                              *renderPass_,
                                              1,
                                              &(*view),
                                              swapChainExtent_.width,
                                              swapChainExtent_.height,
                                              1)));
        }
    }

    void createCommandPools()
    {
        commandPool_          = device_->createCommandPoolUnique(vk::CommandPoolCreateInfo({}, graphicsFamily_));
        transientCommandPool_ = device_->createCommandPoolUnique(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient,
                                      graphicsFamily_));
    }

    void createVertexBuffer()
    {
        vertexBuffer_ = Vkx::createDeviceLocalBuffer(device_.get(),
                                                     physicalDevice_,
                                                     vk::BufferUsageFlagBits::eVertexBuffer,
                                                     vertices,
                                                     sizeof(vertices),
                                                     transientCommandPool_.get(),
                                                     graphicsQueue_);
    }

    void createIndexBuffer()
    {
        indexBuffer_ = Vkx::createDeviceLocalBuffer(device_.get(),
                                                    physicalDevice_,
                                                    vk::BufferUsageFlagBits::eIndexBuffer,
                                                    indices,
                                                    sizeof(indices),
                                                    transientCommandPool_.get(),
                                                    graphicsQueue_);
    }

    void createCommandBuffers()
    {
        vk::ClearValue clearColor(std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f });
        vk::Buffer     vertexBuffers[] = { vertexBuffer_ };
        vk::DeviceSize offsets[]       = { 0 };

        commandBuffers_ = device_->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo(*commandPool_,
                                          vk::CommandBufferLevel::ePrimary,
                                          (uint32_t)swapChainImageViews_.size()));

        int i = 0;
        for (auto & buffer : commandBuffers_)
        {
            buffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
            buffer->beginRenderPass(
                vk::RenderPassBeginInfo(*renderPass_,
                                        *swapChainFramebuffers_[i],
                                        {{ 0, 0 }, swapChainExtent_ },
                                        1,
                                        &clearColor),
                vk::SubpassContents::eInline);
            buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
            buffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
            buffer->bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint16);
            buffer->drawIndexed(6, 1, 0, 0, 0);
            buffer->endRenderPass();
            buffer->end();
            ++i;
        }
    }

    void createSemaphores()
    {
        imageAvailableSemaphores_.reserve(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores_.reserve(MAX_FRAMES_IN_FLIGHT);
        inFlightFences_.reserve(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            imageAvailableSemaphores_.push_back(device_->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
            renderFinishedSemaphores_.push_back(device_->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
            inFlightFences_.push_back(device_->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
        }
    }

    void drawFrame()
    {
        device_->waitForFences(1, &(*inFlightFences_[currentFrame_]), VK_TRUE, std::numeric_limits<uint64_t>::max());

        uint32_t imageIndex;
        try
        {
            vk::ResultValue<uint32_t> result = device_->acquireNextImageKHR(*swapChain_,
                                                                            std::numeric_limits<uint64_t>::max(),
                                                                            *imageAvailableSemaphores_[currentFrame_],
                                                                            nullptr);

            if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
                throw std::runtime_error("drawFrame: failed to acquire swap chain image!");
            imageIndex = result.value;
        }
        catch (vk::OutOfDateKHRError &)
        {
            recreateSwapChain();
            return;
        }

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo         submitInfo(1,
                                          &(*imageAvailableSemaphores_[currentFrame_]),
                                          &waitStage,
                                          1,
                                          &(*commandBuffers_[imageIndex]),
                                          1,
                                          &(*renderFinishedSemaphores_[currentFrame_]));
        device_->resetFences(1, &(*inFlightFences_[currentFrame_]));
        graphicsQueue_.submit(1, &submitInfo, *inFlightFences_[currentFrame_]);

        try
        {
            vk::Result result = presentQueue_.presentKHR(
                vk::PresentInfoKHR(1,
                                   &(*renderFinishedSemaphores_[currentFrame_]),
                                   1,
                                   &(*swapChain_),
                                   &imageIndex));
            if (result == vk::Result::eSuboptimalKHR || framebufferSizeChanged_)
                recreateSwapChain();
        }
        catch (vk::OutOfDateKHRError &)
        {
            recreateSwapChain();
        }

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void resetSwapChain()
    {
        swapChainFramebuffers_.clear();
        commandBuffers_.clear();
        graphicsPipeline_.reset();
        pipelineLayout_.reset();
        renderPass_.reset();
        swapChainImageViews_.clear();
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
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandBuffers();
    }

    std::unique_ptr<Glfwx::Window> window_ = nullptr;
    vk::UniqueInstance instance_;
    vk::DispatchLoaderDynamic dynamicLoader_;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> messenger_;
    uint32_t graphicsFamily_;
    uint32_t presentFamily_;
    vk::PhysicalDevice physicalDevice_;
    vk::UniqueDevice device_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    vk::UniqueSurfaceKHR surface_;
    vk::UniqueSwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    std::vector<vk::UniqueImageView> swapChainImageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    vk::UniqueRenderPass renderPass_;
    vk::UniquePipelineLayout pipelineLayout_;
    vk::UniquePipeline graphicsPipeline_;
    std::vector<vk::UniqueFramebuffer> swapChainFramebuffers_;
    vk::UniqueCommandPool commandPool_;
    vk::UniqueCommandPool transientCommandPool_;
    Vkx::Buffer vertexBuffer_;
    Vkx::Buffer indexBuffer_;
    std::vector<vk::UniqueCommandBuffer> commandBuffers_;
    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores_;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores_;
    std::vector<vk::UniqueFence> inFlightFences_;
    int currentFrame_ = 0;
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
