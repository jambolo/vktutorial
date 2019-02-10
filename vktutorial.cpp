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
    int initializeWindow()
    {
        window_ = new Glfwx::Window(WIDTH, HEIGHT, "vktutorial");
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        return window_->open();
    }
    void initVulkan()
    {
        vk::ApplicationInfo appInfo("Hello Triangle",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    "No Engine",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    VK_API_VERSION_1_0);

        std::vector<char const *> requiredExtensions = Glfwx::requiredInstanceExtensions();
        vk::InstanceCreateInfo    createInfo;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (int)requiredExtensions.size();
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        instance_ = vk::createInstanceUnique(createInfo);

        if (VALIDATION_LAYERS_REQUESTED && !validationLayersSupported()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }
    }

    void mainLoop()
    {
    }

    void cleanup()
    {
        instance_.reset();
        if (window_)
            delete window_;
    }

private:

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;

    bool validationLayersSupported()
    {
        std::vector<vk::LayerProperties> supported = vk::enumerateInstanceLayerProperties();
        for (auto request = VALIDATION_LAYERS[0]; request; ++request)
        {
            bool found = false;
            for (auto const & layer : supported)
            {
                if (strcmp(request, layer.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        return true;
    }

#ifdef NDEBUG
    static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
    static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif
    static char const * const VALIDATION_LAYERS[];

    Glfwx::Window * window_ = nullptr;
    vk::UniqueInstance instance_;
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
        std::cout << (int)extensions.size() << " extensions supported:" << std::endl;
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
