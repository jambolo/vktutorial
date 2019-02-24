#define GLFW_INCLUDE_VULKAN
#include "Glfwx/Glfwx.h"
#include "Vkx/Vkx.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef NDEBUG
static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif

std::vector<char const *> const VALIDATION_LAYERS =
{
    "VK_LAYER_LUNARG_standard_validation"
};

std::vector<char const *> const DEVICE_EXTENSIONS =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

    int initializeWindow()
    {
        window_ = std::make_unique<Glfwx::Window>(WIDTH, HEIGHT, "vktutorial");
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        return window_->create();
    }

    void initializeVulkan()
    {
        if (VALIDATION_LAYERS_REQUESTED && !Vkx::allLayersAvailable(VALIDATION_LAYERS))
            throw std::runtime_error("validation layers requested, but not available!");

        vk::ApplicationInfo appInfo("Hello Triangle",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    "No Engine",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    VK_API_VERSION_1_0);

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
        instance_ = vk::createInstanceUnique(createInfo);

        dynamicLoader_.init(*instance_);

        setupDebugMessenger();

        surface_        = vk::UniqueSurfaceKHR(window_->createSurface(*instance_, nullptr), *instance_);
        physicalDevice_ = firstSuitablePhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFrameBuffers();
        createCommandBuffers();
        createSemaphores();
    }

    std::vector<char const *> myRequiredExtensions()
    {
        std::vector<char const *> requiredExtensions = Glfwx::requiredInstanceExtensions();

        if (VALIDATION_LAYERS_REQUESTED)
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return requiredExtensions;
    }

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

    vk::PhysicalDevice firstSuitablePhysicalDevice()
    {
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
        //         bool suitable = false;
        //         vk::PhysicalDeviceProperties properties = device.getProperties();
        //         vk::PhysicalDeviceFeatures features = device.getFeatures();
        //
        //         if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && features.geometryShader)
        //             suitable = true;
        //
        //         return suitable;
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
        for (auto f = families.begin(); f != families.end(); ++f)
        {
            if (f->queueCount > 0)
            {
                int index = (int)std::distance(families.begin(), f);
                if (f->queueFlags & vk::QueueFlagBits::eGraphics)
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

        swapChain_            = device_->createSwapchainKHRUnique(createInfo);
        swapChainImages_      = device_->getSwapchainImagesKHR(*swapChain_);
        swapChainImageFormat_ = surfaceFormat.format;
        swapChainExtent_      = extent;
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
            uint32_t width, height;

            width  = std::clamp((uint32_t)WIDTH, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            height = std::clamp((uint32_t)HEIGHT, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return { width, height };
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

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

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

    void createFrameBuffers()
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

    void createCommandBuffers()
    {
        commandPool_    = device_->createCommandPoolUnique(vk::CommandPoolCreateInfo({}, graphicsFamily_));
        commandBuffers_ = device_->allocateCommandBuffers(
            vk::CommandBufferAllocateInfo(*commandPool_,
                                          vk::CommandBufferLevel::ePrimary,
                                          (uint32_t)swapChainImageViews_.size()));

        vk::ClearValue clearColor(std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f });
        for (int i = 0; i < (int)commandBuffers_.size(); ++i)
        {
            commandBuffers_[i].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
            commandBuffers_[i].beginRenderPass(
                vk::RenderPassBeginInfo(*renderPass_,
                                        *swapChainFramebuffers_[i],
                                        {{ 0, 0 }, swapChainExtent_ },
                                        1,
                                        &clearColor),
                vk::SubpassContents::eInline);
            commandBuffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
            commandBuffers_[i].draw(3, 1, 0, 0);
            commandBuffers_[i].endRenderPass();
            commandBuffers_[i].end();
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
        device_->resetFences(1, &(*inFlightFences_[currentFrame_]));

        vk::ResultValue<uint32_t> imageIndex = device_->acquireNextImageKHR(*swapChain_,
                                                                            std::numeric_limits<uint64_t>::max(),
                                                                            *imageAvailableSemaphores_[currentFrame_],
                                                                            vk::Fence());

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo         submitInfo(1,
                                          &(*imageAvailableSemaphores_[currentFrame_]),
                                          &waitStage,
                                          1,
                                          &commandBuffers_[imageIndex.value],
                                          1,
                                          &(*renderFinishedSemaphores_[currentFrame_]));
        graphicsQueue_.submit(1, &submitInfo, *inFlightFences_[currentFrame_]);

        presentQueue_.presentKHR(
            vk::PresentInfoKHR(1,
                               &(*renderFinishedSemaphores_[currentFrame_]),
                               1,
                               &(*swapChain_),
                               &imageIndex.value));

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
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
    std::vector<vk::CommandBuffer> commandBuffers_;
    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores_;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores_;
    std::vector<vk::UniqueFence> inFlightFences_;
    int currentFrame_ = 0;
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        Glfwx::init();
        app.run();
        Glfwx::terminate();
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
