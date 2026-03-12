cmake_minimum_required(VERSION 3.20)
project(MistNative VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. Fetch pre-compiled wgpu-native (WebGPU for Desktop)
FetchContent_Declare(
    wgpu_native
    URL https://github.com/gfx-rs/wgpu-native/releases/latest/download/wgpu-windows-x86_64-msvc-release.zip
)
FetchContent_MakeAvailable(wgpu_native)

# 3. Build our DLL
add_library(mist_native SHARED 
    src/window.cpp
)

# 4. Link Directories and Libraries
target_include_directories(mist_native PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${wgpu_native_SOURCE_DIR}/include
)

# Link GLFW and the WebGPU library
target_link_libraries(mist_native PRIVATE 
    glfw 
    ${wgpu_native_SOURCE_DIR}/lib/wgpu_native.dll.lib
)

# 5. Automatically copy wgpu_native.dll next to our mist_native.dll 
add_custom_command(TARGET mist_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${wgpu_native_SOURCE_DIR}/lib/wgpu_native.dll"
    $<TARGET_FILE_DIR:mist_native>
)