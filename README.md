# 🛩️ Super Tucano Engine

A modern, high-performance, Data-Oriented 3D Game Engine built from scratch in **C++23** and **Vulkan**. 
Designed to be robust, scalable, and engineered with AAA architecture patterns.

## 🚀 Features (Current Status)
* **Vulkan 1.3 Backend**: Engineered using modern graphics API standards with zero-overhead wrappers.
* **C++23 Modernity**: Fully utilizes the latest standard library features and strict compiler compliance.
* **CMake FetchContent**: Fully automated dependency management. Just clone and configure.
* **Runtime Shader Compilation**: Seamless GLSL -> SPIR-V compilation at runtime using `shaderc` to support hot-reloading.
* **3D World Foundation**:
  * FPS Camera Navigation (WASD + Mouse Look).
  * High-performance 3D overlapping with Depth Buffering.
  * Push Constants for blazing-fast `ModelMatrix` draw calls.
  * Uniform Buffer Objects (UBOs) for View & Projection matrices.
  * Math powered by `GLM`.
* **Vulkan Memory Allocator (VMA)**: State-of-the-art memory management for Vulkan buffers and images.
* **Decoupled Architecture**: Strictly separated `TucanoCore` static library and `SandboxApp` runtime.

## 🛠️ Tech Stack & Dependencies
The engine downloads and compiles all dependencies dynamically via CMake:
* **GLFW**: Cross-platform Window and Input handling.
* **spdlog**: High-performance asynchronous logging.
* **GLM**: Mathematics library for graphics software.
* **volk**: Meta loader for Vulkan API.
* **VulkanMemoryAllocator (VMA)**: Advanced memory allocation.
* **shaderc**: Runtime shader compilation.
* **stb_image**: Image loading (Planned for Milestone 3).
* **EnTT**: Fast and reliable Entity-Component-System (Planned for Milestone 3).

## 🏗️ How to Build
Super Tucano Engine uses modern CMake. To build it locally on your machine, you'll need:
1. **C++23 Compiler** (MSVC, GCC, or Clang).
2. **CMake 3.25+**.
3. **Vulkan SDK** installed natively on your OS.

### Windows (Powershell)
```powershell
git clone https://github.com/SombraDev07/SuperTucanoEngine.git
cd SuperTucanoEngine

# Configure the project
cmake -B build -S .

# Build the engine and sandbox
cmake --build build --config Debug

# Run the engine
cd build\bin\Debug
.\SandboxApp.exe
```

## 🎮 Controls (SandboxApp)
* **Hold Right Mouse Button**: Enter Free Look mode.
* **W, A, S, D**: Move Forward, Left, Backward, and Right.
* **E / Q**: Elevate Up / Descend Down.
* **ESC**: Exit Sandbox.

## 📜 Roadmap
- [x] **Milestone 1**: Core Architecture & Vulkan Init (The Triangle).
- [x] **Milestone 2**: World Runtime Foundation (Camera, 3D Meshes, Push Constants, Depth).
- [ ] **Milestone 3**: ECS Integration & Asset Pipeline (glTF, Textures, EnTT).
- [ ] **Milestone 4**: Physically Based Rendering (PBR) & Lighting.
- [ ] **Milestone 5**: Advanced Rendering (Render Graph, Atmospheric Scattering).

---
*Developed with focus on Performance and Clean Architecture.*
