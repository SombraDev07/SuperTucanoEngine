add_library(TucanoCore STATIC
    src/Core/Logger.cpp
    src/Core/Window.cpp
    src/Core/Input.cpp
    src/Camera/Camera.cpp
    src/Camera/FPSCameraController.cpp
    src/Mesh/Mesh.cpp
    src/World/World.cpp
    src/Render/VulkanContext.cpp
    src/Render/ShaderCompiler.cpp
    src/Render/Renderer.cpp
    src/Render/Texture2D.cpp
    src/Render/Material.cpp
    src/Asset/ModelLoader.cpp
    vendor/ufbx/ufbx.c
)

target_include_directories(TucanoCore PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
)

target_link_libraries(TucanoCore PUBLIC
    glfw
    glm::glm
    EnTT::EnTT
    spdlog::spdlog
    volk::volk
    VulkanMemoryAllocator
    Vulkan::Vulkan
    Vulkan::shaderc_combined
    stb
    cgltf
)

# Enable C++23
target_compile_features(TucanoCore PUBLIC cxx_std_23)
