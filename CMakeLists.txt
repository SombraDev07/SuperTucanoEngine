cmake_minimum_required(VERSION 3.25)

project(TucanoEngine VERSION 0.1.0 LANGUAGES C CXX)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global properties
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find Vulkan (we need it installed on the system, specifically for shaderc and validation layers)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

# Setup FetchContent
include(FetchContent)

# 1. spdlog (Logging)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# 2. GLFW (Windowing)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 3. GLM (Math)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# 4. EnTT (ECS)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.1
)
FetchContent_MakeAvailable(entt)

# 5. VulkanMemoryAllocator (VMA)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
)
set(VMA_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vma)

# 6. volk (Vulkan Meta Loader)
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(volk)

# 7. stb_image (Texture loading)
# Because STB is a single header, FetchContent just pulls the repo, we'll create an interface library for it.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# Output directories for clean build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add subdirectories
add_subdirectory(TucanoCore)
add_subdirectory(SandboxApp)
