#define GLFW_INCLUDE_VULKAN
#include "Glfwx/Glfwx.h"

#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <iostream>
#include <stdexcept>

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

#ifdef NDEBUG
    static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
    static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;

    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;
        bool isComplete = false;
    };

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice const & device)
    {
        std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

        QueueFamilyIndices indices;
        for (int i = 0; i < (int)families.size(); ++i)
        {
            vk::QueueFamilyProperties const & f = families[i];
            if (f.queueCount > 0 && f.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
                indices.isComplete     = true;
                break;
            }
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
        instance_ = vk::createInstanceUnique(createInfo);

        dynamicLoader_.init(*instance_);

        if (VALIDATION_LAYERS_REQUESTED && !validationLayersAvailable())
            throw std::runtime_error("validation layers requested, but not available!");

        setupDebugMessenger();

        vk::PhysicalDevice physicalDevice = firstSuitableDevice();
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
        return findQueueFamilies(device).isComplete;
    }

    vk::PhysicalDevice firstSuitableDevice()
    {
        std::vector<vk::PhysicalDevice> physicalDevices = instance_->enumeratePhysicalDevices();
        for (auto const & device : physicalDevices)
        {
            if (isSuitable(device))
                return device;
        }

        throw std::runtime_error("failed to find a suitable GPU!");
    }

    void mainLoop()
    {
    }

    void cleanup()
    {
        messenger_.reset();
        instance_.reset();
        if (window_)
            delete window_;
    }

    bool validationLayersAvailable()
    {
        std::vector<vk::LayerProperties> available = vk::enumerateInstanceLayerProperties();
        std::cout << (int)available.size() << " layers available:" << std::endl;
        for (auto const & layer : available)
        {
            std::cout << "\t" << layer.layerName << std::endl;
        }

        for (auto request = &VALIDATION_LAYERS[0]; *request; ++request)
        {
            std::cout << "Requesting " << *request << ": ";
            bool found = false;
            for (auto const & layer : available)
            {
                if (strcmp(*request, layer.layerName) == 0)
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
        std::cerr << "validation says: ("
                  << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity))  << ") ["
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << "] - "
                  << pCallbackData->pMessage
                  << std::endl;
        return VK_FALSE;
    }

    void setupDebugMessenger()
    {
        if (!VALIDATION_LAYERS_REQUESTED)
            return;
        vk::DebugUtilsMessengerCreateInfoEXT info
        {
            {},
            (vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo),
            (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance),
            debugCallback
        };
        messenger_ = instance_->createDebugUtilsMessengerEXTUnique(info, nullptr, dynamicLoader_);
    }

    Glfwx::Window * window_ = nullptr;
    vk::UniqueInstance instance_;
    vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> messenger_;
    vk::DispatchLoaderDynamic dynamicLoader_;

    static char const * const VALIDATION_LAYERS[];
};

char const * const HelloTriangleApplication::VALIDATION_LAYERS[] =
{
    "VK_LAYER_LUNARG_standard_validation",
    nullptr
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
