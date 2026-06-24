# Tucano Engine - Architectural Context

## Overview
Tucano Engine is a modern AAA-focused game engine written from scratch in C++23. It utilizes Vulkan as its primary graphics API, prioritizing high fidelity, excellent performance, and modern features like Hybrid Ray Tracing and advanced Atmospheric Scattering (based on Eric Bruneton's Precomputed Atmospheric Scattering).

## Core Principles
1. **Modern C++**: Heavy use of C++20/23 features (concepts, ranges, modules eventually) to write clean, expressive code.
2. **Data-Oriented Design**: Architecture centered around contiguous memory layouts. Entity Component System (ECS) powered by EnTT is the backbone of the gameplay and rendering logic.
3. **Explicit Rendering**: Using Vulkan with a custom Render Graph architecture to allow complex, multi-pass rendering pipelines without state-tracking overhead.
4. **Fast Iteration**: Hot-reloading of shaders and gameplay code where possible.

## Current Milestone: Base Architecture (Vulkan + CMake)
We are currently establishing the foundation:
- **Build System**: CMake with `FetchContent` for frictionless dependency management.
- **Dependencies**: 
  - `GLFW` (Windowing)
  - `volk` (Vulkan Meta Loader)
  - `VMA` (Vulkan Memory Allocator)
  - `shaderc` (Runtime GLSL/HLSL to SPIR-V compilation)
  - `spdlog` (High-performance logging)
  - `GLM` (Mathematics)
  - `EnTT` (ECS)
  - `stb_image` (Texture loading)
- **Module Structure**: Split between `TucanoCore` (Static Library) and `SandboxApp` (Executable).

## Future Milestones
- **Milestone 2**: PBR Materials and Asset Pipeline (glTF loading, basic lighting).
- **Milestone 3**: Render Graph implementation.
- **Milestone 4**: Advanced Atmospheric Scattering (Bruneton + modern improvements).
- **Milestone 5**: Hybrid Ray Tracing and advanced GI techniques.
