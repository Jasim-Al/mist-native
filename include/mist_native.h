#pragma once

#if defined(_WIN32)
#define MIST_API __declspec(dllexport)
#else
#define MIST_API __attribute__((visibility("default")))
#endif

extern "C"
{
    MIST_API void mist_init_window(int width, int height, const char *title);
    MIST_API void mist_resize_window(int width, int height);
    MIST_API void mist_poll_events();
    MIST_API bool mist_window_should_close();
    MIST_API void mist_swap_buffers();

    // Final Input & State Additions
    MIST_API bool mist_get_key(int key);
    MIST_API void mist_get_mouse_pos(double *x, double *y);
    MIST_API void mist_set_input_mode(int mode, int value);

    // Graphics
    MIST_API void mist_set_mesh_color(float r, float g, float b, float a);
    MIST_API void mist_set_vertex_buffer(const float *vertices, int vertexCount);
    MIST_API void mist_set_transform(const float *matrix);
}