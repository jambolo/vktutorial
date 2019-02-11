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
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Copy the DLLs to next to the executable
if (WIN32 AND BUILD_SHARED_LIBS)
    find_path(glfw3_DIR "lib/glfw3.lib" PATHS "C:/Program Files/glfw" CACHE)
    if (glfw3_DIR)
        set(glfw3_LIBRARIES "${glfw3_DIR}/lib")
    else()
        message(WARNING "glfw3 was not found.")
    endif()
    add_custom_command(TARGET vktutorial POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${glfw3_LIBRARIES}/glfw3.dll" $<TARGET_FILE_DIR:vktutorial>)
endif()

add_subdirectory(Glfwx)
