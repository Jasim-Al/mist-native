# Mist-Native 🌿
The high-performance C++ core for the Mist Game Engine.

## Overview
Mist-Native is a lightweight wrapper around **WebGPU (wgpu-native)** and **GLFW**. It provides a flat C API designed to be consumed via Foreign Function Interface (FFI), specifically optimized for the **Bun.js** runtime.

## Core Architecture


The core design principle is "Dumb Backend, Smart Frontend." The C++ layer handles:
* OS Window creation and event polling (GLFW).
* GPU device initialization and memory management (WebGPU).
* High-speed vertex and uniform buffer updates.

## API Reference (FFI)

| Function | Description |
| :--- | :--- |
| `mist_init_window` | Initializes GLFW and creates a WebGPU-ready window. |
| `mist_set_shader` | Hot-reloads WGSL shader code into the pipeline. |
| `mist_set_vertex_buffer` | Maps a Float32Array to GPU vertex memory. |
| `mist_set_transform` | Updates the 4x4 MVP Camera Matrix. |
| `mist_set_mesh_color` | Updates the uniform color buffer. |
| `mist_swap_buffers` | Executes the render pass and presents to the screen. |

## Build Instructions
1. Ensure **CMake** and **Visual Studio (MSVC)** are installed.
2. Run the following in the root:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build . --config Debug