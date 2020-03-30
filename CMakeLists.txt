cmake_minimum_required (VERSION 3.10)
project(vktutorial CXX)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)

find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Wx REQUIRED)
find_package(Msxmlx REQUIRED)
find_package(Misc REQUIRED)
#find_package(Glfwx REQUIRED)
#find_package(Vkx REQUIRED)

set(VKTUTORIAL_INCLUDE_PATHS
    .
)

set(VKTUTORIAL_SOURCES
    vktutorial.cpp
)
source_group(Sources FILES ${VKTUTORIAL_SOURCES})

set(VKTUTORIAL_INCLUDE_PATHS
    .
)

set(VKTUTORIAL_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
)
source_group(Shaders FILES ${VKTUTORIAL_SHADER_SOURCES})

add_executable(vktutorial ${VKTUTORIAL_SOURCES})
target_link_libraries(vktutorial
    Glfwx
    Vkx
    glm::glm
    Vulkan::Vulkan
    nlohmann_json::nlohmann_json
)
target_include_directories(vktutorial PRIVATE ${VKTUTORIAL_INCLUDE_PATHS})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_definitions(vktutorial
    PRIVATE
        -DNOMINMAX
        -DWIN32_LEAN_AND_MEAN
        -DVC_EXTRALEAN
        -D_CRT_SECURE_NO_WARNINGS
        -D_SECURE_SCL=0
        -D_SCL_SECURE_NO_WARNINGS
)

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

# Compile the shaders if necessary
find_program(GLSLANGVALIDATOR NAMES glslangValidator glslangValidator.exe)
foreach(GLSL ${VKTUTORIAL_SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${GLSLANGVALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(
    shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(vktutorial shaders)

add_custom_command(TARGET vktutorial POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/textures/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/textures"
        "${PROJECT_BINARY_DIR}/textures"
)

add_custom_command(TARGET vktutorial POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/models/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/models"
        "${PROJECT_BINARY_DIR}/models"
)

add_subdirectory(Glfwx)
add_subdirectory(Vkx)
