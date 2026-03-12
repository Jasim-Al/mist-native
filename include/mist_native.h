#pragma once

#if defined(_WIN32)
#define MIST_API __declspec(dllexport)
#else
#define MIST_API __attribute__((visibility("default")))
#endif

extern "C"
{
    MIST_API void mist_init_window(int width, int height, const char *title);
    MIST_API void mist_poll_events();
    MIST_API bool mist_window_should_close();
    MIST_API void mist_swap_buffers();
    MIST_API void mist_set_mesh_color(float r, float g, float b, float a);
    MIST_API void mist_set_vertex_buffer(const float *vertices, int vertexCount);

    // NEW: Accept a 16-float (4x4) matrix from JavaScript
    MIST_API void mist_set_transform(const float *matrix);

    MIST_API void mist_set_shader(const char *shaderSource);
}