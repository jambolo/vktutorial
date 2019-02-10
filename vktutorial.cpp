#include "Glfwx/Glfwx.h"

#include <GLFW/glfw3.h>
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
        window_ = new Glfwx::Window(WIDTH, HEIGHT, "vktutorial", nullptr, nullptr);
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        return window_->open();
    }
    void initVulkan()
    {
        createInstance();
    }

    void mainLoop()
    {
    }

    void cleanup()
    {
        if (window_)
            delete window_;
    }

private:

    void createInstance()
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
        instance_ = vk::createInstance(createInfo);
    }

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;

    Glfwx::Window * window_  = nullptr;
    vk::Instance instance_;
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
        Glfwx::start();
        app.run();
        Glfwx::finish();
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
