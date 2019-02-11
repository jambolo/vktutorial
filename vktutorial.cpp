#define GLFW_INCLUDE_VULKAN
#include "Glfwx/Glfwx.h"

#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <iostream>
#include <stdexcept>
#include <set>

template <typename T>
class optional
{
public:
    optional() = default;
    optional(T const & value) : value_(value), hasValue_(true)
    {
    }
    optional & operator =(T const & rhs)
    {
        value_ = rhs;
        hasValue_ = true;
        return *this;
    }
    bool hasValue() const
    {
        return hasValue_;
    }
    T value() const
    {
        if (hasValue_)
            return value_;
        else
            throw std::exception("bad_optional_access");
    }
private:
    T value_;
    bool hasValue_ = false;
};

#ifdef NDEBUG
static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif

std::vector<char const *> const VALIDATION_LAYERS =
{
    "VK_LAYER_LUNARG_standard_validation",
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initializeWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;

    struct QueueFamilyIndices
    {
        optional<uint32_t> graphicsFamily;
        optional<uint32_t> presentFamily;
        bool isComplete() const
        {
            return graphicsFamily.hasValue() && presentFamily.hasValue();
        }
    };

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice const & device)
    {
        std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

        QueueFamilyIndices indices;
        for (int i = 0; i < (int)families.size(); ++i)
        {
            vk::QueueFamilyProperties const & f = families[i];
            if (f.queueCount > 0 && f.queueFlags & vk::QueueFlagBits::eGraphics)
                indices.graphicsFamily = i;
            VkBool32 presentSupport = device.getSurfaceSupportKHR(i, surface_);
            if (f.queueCount > 0 && presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;
        }
        return indices;
    }

    int initializeWindow()
    {
        window_ = new Glfwx::Window(WIDTH, HEIGHT, "vktutorial");
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        return window_->create();
    }

    void initVulkan()
    {
        vk::ApplicationInfo appInfo("Hello Triangle",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    "No Engine",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    VK_API_VERSION_1_0);

        std::vector<char const *> requiredExtensions = myRequiredExtensions();
        vk::InstanceCreateInfo    createInfo;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = (uint32_t)requiredExtensions.size();
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        instance_ = vk::createInstance(createInfo);

        dynamicLoader_.init(instance_);

        if (VALIDATION_LAYERS_REQUESTED && !validationLayersAvailable())
            throw std::runtime_error("validation layers requested, but not available!");

        setupDebugMessenger();

        surface_ = window_->createSurface(instance_, nullptr);

        vk::PhysicalDevice physicalDevice = firstSuitablePhysicalDevice();

        createLogicalDevice(physicalDevice);
    }


    void createLogicalDevice(vk::PhysicalDevice const & physicalDevice)
    {
            QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

            // De-duplicate the family indices
            std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            float priority = 1.0f;
            std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
            for (auto const & i : uniqueQueueFamilies)
            {
                queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), i, 1, &priority);
            }

            vk::PhysicalDeviceFeatures deviceFeatures;
            vk::DeviceCreateInfo       createInfo({},
                                                  (uint32_t)queueCreateInfos.size(), queueCreateInfos.data(),
                                                  0, nullptr,
                                                  0, nullptr,
                                                  &deviceFeatures);
            if (VALIDATION_LAYERS_REQUESTED)
            {
                createInfo.setPpEnabledLayerNames(VALIDATION_LAYERS.data());
                createInfo.setEnabledLayerCount(static_cast<uint32_t>(VALIDATION_LAYERS.size()));
            }

            device_ = physicalDevice.createDevice(createInfo);
            graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
            presentQueue_ = device_.getQueue(indices.presentFamily.value(), 0);
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
        return findQueueFamilies(device).isComplete();
    }

    vk::PhysicalDevice firstSuitablePhysicalDevice()
    {
        std::vector<vk::PhysicalDevice> physicalDevices = instance_.enumeratePhysicalDevices();
        for (auto const & device : physicalDevices)
        {
            if (isSuitable(device))
                return device;
        }

        throw std::runtime_error("failed to find a suitable GPU!");
    }

    bool validationLayersAvailable()
    {
        std::vector<vk::LayerProperties> available = vk::enumerateInstanceLayerProperties();
        std::cout << (int)available.size() << " layers available:" << std::endl;
        for (auto const & layer : available)
        {
            std::cout << "\t" << layer.layerName << std::endl;
        }

        for (auto const & request : VALIDATION_LAYERS)
        {
            std::cout << "Requesting " << request << ": ";
            bool found = false;
            for (auto const & layer : available)
            {
                if (strcmp(request, layer.layerName) == 0)
                {
                    std::cout << "found" << std::endl;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                std::cout << "not found" << std::endl;
                return false;
            }
        }
        return true;
    }

    std::vector<char const *> myRequiredExtensions()
    {
        std::vector<char const *> requiredExtensions = Glfwx::requiredInstanceExtensions();

        if (VALIDATION_LAYERS_REQUESTED)
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return requiredExtensions;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
                                                        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
                                                        void *                                       pUserData)
    {
        std::cerr << "(" << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity))  << ")"
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << " - "
                  << pCallbackData->pMessage
                  << std::endl;
        return VK_FALSE;
    }

    void setupDebugMessenger()
    {
        if (!VALIDATION_LAYERS_REQUESTED)
            return;
        vk::DebugUtilsMessengerCreateInfoEXT info({},
                                                  (vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo),
                                                  (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance),
                                                  debugCallback);
        messenger_ = instance_.createDebugUtilsMessengerEXT(info, nullptr, dynamicLoader_);
    }

    void cleanup()
    {
        device_.destroy();
        instance_.destroySurfaceKHR(surface_);
        instance_.destroy(messenger_, nullptr, dynamicLoader_);
        instance_.destroy();
        if (window_)
            delete window_;
    }

    void mainLoop()
    {
    }

    Glfwx::Window * window_ = nullptr;
    vk::Instance instance_;
    vk::DispatchLoaderDynamic dynamicLoader_;
    vk::DebugUtilsMessengerEXT messenger_;
    vk::Device device_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    VkSurfaceKHR surface_;
};

int main()
{
    {
        std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties(nullptr);
        std::cout << (int)extensions.size() << " extensions available:" << std::endl;
        for (auto const e : extensions)
        {
            std::cout << "\t" << e.extensionName << std::endl;
        }
    }

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
