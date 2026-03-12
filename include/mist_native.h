#pragma once

// Platform-specific export macros for creating the .dll
#if defined(_WIN32)
#define MIST_API __declspec(dllexport)
#else
#define MIST_API __attribute__((visibility("default")))
#endif

extern "C"
{
    // These are the exact functions Bun will be able to call!
    MIST_API void mist_init_window(int width, int height, const char *title);
    MIST_API void mist_poll_events();
    MIST_API bool mist_window_should_close();
    MIST_API void mist_swap_buffers();
}