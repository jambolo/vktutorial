cmake_minimum_required (VERSION 3.10)
project(vktutorial CXX)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)

find_package(glm)
find_package(Vulkan)

set(VKTUTORIAL_SOURCES
    vktutorial.cpp
)
source_group(Sources FILES ${VKTUTORIAL_SOURCES})

set(VKTUTORIAL_INCLUDE_PATHS
    .
)

add_executable(vktutorial ${VKTUTORIAL_SOURCES})
target_link_libraries(vktutorial glm Vulkan::Vulkan Glfwx)
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
