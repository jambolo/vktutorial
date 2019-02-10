cmake_minimum_required (VERSION 3.10)
project(vktutorial CXX)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)

find_path(glfw_DIR "lib/glfw3.lib" PATHS "C:/Program Files/glfw" CACHE)
if (glfw_DIR)
    set(glfw_LIBRARIES "${glfw_DIR}/lib")
    set(glfw_INCLUDE_DIRS "${glfw_DIR}/include")
    message(STATUS "glfw_DIR = ${glfw_DIR}")
    message(STATUS "glfw_LIBRARIES = ${glfw_LIBRARIES}")
    message(STATUS "glfw_INCLUDE_DIRS = ${glfw_INCLUDE_DIRS}")
    if (BUILD_SHARED_LIBS)
        add_library(glfw SHARED IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_LOCATION "${glfw_LIBRARIES}/glfw3.dll"
            IMPORTED_IMPLIB "${glfw_LIBRARIES}/glfw3dll.lib")
    else()
        add_library(glfw STATIC IMPORTED)
        set_target_properties(glfw PROPERTIES IMPORTED_LOCATION "${glfw_LIBRARIES}/glfw3.lib")
    endif()
    target_include_directories(glfw INTERFACE ${glfw_INCLUDE_DIRS})
else()
    message(STATUS "glfw was not found.")
endif()

find_package(glm)
if (glm_FOUND)
    message(STATUS "glm_DIR = ${glm_DIR}")
    message(STATUS "GLM_INCLUDE_DIRS = ${GLM_INCLUDE_DIRS}")
else()
    message(WARNING "glm was not found.")
endif()

find_package(Vulkan)
if (Vulkan_FOUND)
    message(STATUS "Vulkan_INCLUDE_DIRS = ${Vulkan_INCLUDE_DIRS}")
    message(STATUS "Vulkan_LIBRARIES = ${Vulkan_LIBRARIES}")
    message(STATUS "Vulkan_INCLUDE_DIR = ${Vulkan_INCLUDE_DIR}")
    message(STATUS "Vulkan_LIBRARY = ${Vulkan_LIBRARY}")
else()
    message(WARNING "Vulkan was not found.")
endif()

set(VKTUTORIAL_SOURCES
    vktutorial.cpp
)
source_group(Sources FILES ${VKTUTORIAL_SOURCES})

set(VKTUTORIAL_INCLUDE_PATHS
    .
)

add_executable(vktutorial ${VKTUTORIAL_SOURCES})
target_link_libraries(vktutorial glfw glm Vulkan::Vulkan Glfwx)
target_compile_definitions(vktutorial
    PRIVATE
        -DNOMINMAX
        -DWIN32_LEAN_AND_MEAN
        -DVC_EXTRALEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_SECURE_SCL=0
        -D_SCL_SECURE_NO_WARNINGS
)
target_include_directories(vktutorial PRIVATE ${VKTUTORIAL_INCLUDE_PATHS})

if (BUILD_SHARED_LIBS)
    add_custom_command(TARGET vktutorial POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${glfw_LIBRARIES}/glfw3.dll" $<TARGET_FILE_DIR:vktutorial>)
endif()

add_subdirectory(Glfwx)
