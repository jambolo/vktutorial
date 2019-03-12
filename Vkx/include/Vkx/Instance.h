#pragma once

#if !defined(VKX_INSTANCE_H)
#define VKX_INSTANCE_H

#include <vulkan/vulkan.hpp>

namespace Vkx
{
//! A RAII extension to vk::Instance.
class Instance : private vk::Instance
{
public:
    //! Constructor.
    Instance(vk::InstanceCreateInfo const & info);

    ~Instance();

    using vk::Instance::operator ==;
    using vk::Instance::operator !=;
    using vk::Instance::operator <;

    using vk::Instance::enumeratePhysicalDevices;
    using vk::Instance::getProcAddr;

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    using vk::Instance::createAndroidSurfaceKHR;
    using vk::Instance::createAndroidSurfaceKHRUnique;
#endif /*VK_USE_PLATFORM_ANDROID_KHR*/

    using vk::Instance::createDisplayPlaneSurfaceKHR;
    using vk::Instance::createDisplayPlaneSurfaceKHRUnique;

#ifdef VK_USE_PLATFORM_VI_NN
    using vk::Instance::createViSurfaceNN;
    using vk::Instance::createViSurfaceNNUnique;
#endif /*VK_USE_PLATFORM_VI_NN*/

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    using vk::Instance::createWaylandSurfaceKHR;
    using vk::Instance::createWaylandSurfaceKHRUnique;
#endif /*VK_USE_PLATFORM_WAYLAND_KHR*/

#ifdef VK_USE_PLATFORM_WIN32_KHR
    using vk::Instance::createWin32SurfaceKHR;
    using vk::Instance::createWin32SurfaceKHRUnique;
#endif /*VK_USE_PLATFORM_WIN32_KHR*/

#ifdef VK_USE_PLATFORM_XLIB_KHR
    using vk::Instance::createXlibSurfaceKHR;
    using vk::Instance::createXlibSurfaceKHRUnique;
#endif /*VK_USE_PLATFORM_XLIB_KHR*/

#ifdef VK_USE_PLATFORM_XCB_KHR
    using vk::Instance::createXcbSurfaceKHR;
    using vk::Instance::createXcbSurfaceKHRUnique;
#endif /*VK_USE_PLATFORM_XCB_KHR*/

#ifdef VK_USE_PLATFORM_FUCHSIA
    using vk::Instance::createImagePipeSurfaceFUCHSIA;
    using vk::Instance::createImagePipeSurfaceFUCHSIAUnique;
#endif /*VK_USE_PLATFORM_FUCHSIA*/

    using vk::Instance::createDebugReportCallbackEXT;
    using vk::Instance::createDebugReportCallbackEXTUnique;

    using vk::Instance::debugReportMessageEXT;

    using vk::Instance::enumeratePhysicalDeviceGroups;

    using vk::Instance::enumeratePhysicalDeviceGroupsKHR;

#ifdef VK_USE_PLATFORM_IOS_MVK
    using vk::Instance::createIOSSurfaceMVK;
    using vk::Instance::createIOSSurfaceMVKUnique;
#endif /*VK_USE_PLATFORM_IOS_MVK*/

#ifdef VK_USE_PLATFORM_MACOS_MVK
    using vk::Instance::createMacOSSurfaceMVK;
    using vk::Instance::createMacOSSurfaceMVKUnique;
#endif /*VK_USE_PLATFORM_MACOS_MVK*/

    using vk::Instance::createDebugUtilsMessengerEXT;
    using vk::Instance::createDebugUtilsMessengerEXTUnique;

    using vk::Instance::submitDebugUtilsMessageEXT;

    using vk::Instance::operator VkInstance;
};
} // namespace Vkx

#endif // !defined(VKX_INSTANCE_H)
