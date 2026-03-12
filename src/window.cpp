#include "mist_native.h"
#include <iostream>

// Include the standard WebGPU C headers
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

static GLFWwindow *g_window = nullptr;
static WGPUInstance g_instance = nullptr;

extern "C"
{

    MIST_API void mist_init_window(int width, int height, const char *title)
    {
        // 1. Initialize the Windowing System
        if (!glfwInit())
        {
            std::cerr << "[Mist Native] Failed to initialize GLFW" << std::endl;
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Crucial for WebGPU
        g_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!g_window)
        {
            std::cerr << "[Mist Native] Failed to create window" << std::endl;
            glfwTerminate();
            return;
        }

        // 2. Initialize the WebGPU Instance
        WGPUInstanceDescriptor desc = {};
        desc.nextInChain = nullptr;
        g_instance = wgpuCreateInstance(&desc);

        if (!g_instance)
        {
            std::cerr << "[Mist Native] CRITICAL: Failed to create WebGPU Instance!" << std::endl;
        }
        else
        {
            std::cout << "[Mist Native] Native window and WebGPU Instance created successfully!" << std::endl;
        }
    }

    MIST_API void mist_poll_events()
    {
        glfwPollEvents();
    }

    MIST_API bool mist_window_should_close()
    {
        if (!g_window)
            return true;
        return glfwWindowShouldClose(g_window);
    }

    MIST_API void mist_swap_buffers()
    {
        // We will hook this up to the WebGPU SwapChain next!
    }
}