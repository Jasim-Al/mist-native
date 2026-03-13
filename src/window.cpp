#include "mist_native.h"
#include <iostream>
#include <cstring>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <webgpu/webgpu.h>

// --- Global State ---
static GLFWwindow *g_window = nullptr;
static WGPUInstance g_instance = nullptr;
static WGPUSurface g_surface = nullptr;
static WGPUAdapter g_adapter = nullptr;
static WGPUDevice g_device = nullptr;
static WGPUQueue g_queue = nullptr;
static WGPURenderPipeline g_pipeline = nullptr;

static WGPUBuffer g_colorBuffer = nullptr;
static WGPUBuffer g_transformBuffer = nullptr;
static WGPUBindGroup g_bindGroup = nullptr;

static WGPUBuffer g_vertexBuffer = nullptr;
static uint32_t g_vertexCount = 0;

static WGPUTextureView g_depthView = nullptr;

// --- WebGPU Callbacks ---
static void OnAdapterRequest(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2)
{
    if (status == WGPURequestAdapterStatus_Success)
        g_adapter = adapter;
}

static void OnDeviceRequest(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2)
{
    if (status == WGPURequestDeviceStatus_Success)
        g_device = device;
}

// --- Internal GLFW Resize Callback ---
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // This allows the C++ side to handle the resize immediately
    // even if the JS thread is busy.
    mist_resize_window(width, height);
}

extern "C"
{

    MIST_API void mist_get_mouse_pos(double *x, double *y)
    {
        if (g_window)
            glfwGetCursorPos(g_window, x, y);
    }

    MIST_API void mist_set_input_mode(int mode, int value)
    {
        if (g_window)
            glfwSetInputMode(g_window, mode, value);
    }

    MIST_API bool mist_get_key(int key)
    {
        if (!g_window)
            return false;
        return glfwGetKey(g_window, key) == GLFW_PRESS;
    }

    MIST_API void mist_resize_window(int width, int height)
    {
        if (!g_device || !g_surface || width <= 0 || height <= 0)
            return;

        WGPUSurfaceConfiguration config = {};
        config.device = g_device;
        config.format = WGPUTextureFormat_BGRA8Unorm;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.width = (uint32_t)width;
        config.height = (uint32_t)height;
        config.presentMode = WGPUPresentMode_Fifo;
        config.alphaMode = WGPUCompositeAlphaMode_Auto;
        wgpuSurfaceConfigure(g_surface, &config);

        if (g_depthView)
            wgpuTextureViewRelease(g_depthView);

        WGPUTextureDescriptor depthDesc = {};
        depthDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthDesc.dimension = WGPUTextureDimension_2D;
        depthDesc.size = {(uint32_t)width, (uint32_t)height, 1};
        depthDesc.format = WGPUTextureFormat_Depth24Plus;
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 1;
        WGPUTexture depthTexture = wgpuDeviceCreateTexture(g_device, &depthDesc);
        g_depthView = wgpuTextureCreateView(depthTexture, nullptr);
        wgpuTextureRelease(depthTexture);

        std::cout << "[Mist] Internal Resize: " << width << "x" << height << std::endl;
    }

    MIST_API void mist_init_window(int width, int height, const char *title)
    {
        if (!glfwInit())
            return;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        g_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        // Register the resize callback
        glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);

        WGPUInstanceDescriptor instDesc = {};
        g_instance = wgpuCreateInstance(&instDesc);

        WGPUSurfaceSourceWindowsHWND hwndDesc = {};
        hwndDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        hwndDesc.hinstance = GetModuleHandle(nullptr);
        hwndDesc.hwnd = glfwGetWin32Window(g_window);

        WGPUSurfaceDescriptor surfaceDesc = {};
        surfaceDesc.nextInChain = (const WGPUChainedStruct *)&hwndDesc;
        g_surface = wgpuInstanceCreateSurface(g_instance, &surfaceDesc);

        WGPURequestAdapterOptions adapterOpts = {};
        adapterOpts.compatibleSurface = g_surface;

        WGPURequestAdapterCallbackInfo adapterCb = {};
        adapterCb.callback = OnAdapterRequest;
        wgpuInstanceRequestAdapter(g_instance, &adapterOpts, adapterCb);

        WGPUDeviceDescriptor deviceDesc = {};
        WGPURequestDeviceCallbackInfo deviceCb = {};
        deviceCb.callback = OnDeviceRequest;
        wgpuAdapterRequestDevice(g_adapter, &deviceDesc, deviceCb);

        g_queue = wgpuDeviceGetQueue(g_device);

        // Initial resize call to setup surface and depth buffer
        mist_resize_window(width, height);

        WGPUBufferDescriptor bufDesc = {};
        bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bufDesc.size = 16;
        g_colorBuffer = wgpuDeviceCreateBuffer(g_device, &bufDesc);
        bufDesc.size = 64;
        g_transformBuffer = wgpuDeviceCreateBuffer(g_device, &bufDesc);

        const char *wgslSource = R"(
        @group(0) @binding(0) var<uniform> u_color: vec4f;
        @group(0) @binding(1) var<uniform> u_transform: mat4x4f;
        struct VertexInput { @location(0) position: vec3f };
        struct VertexOutput { @builtin(position) clip_pos: vec4f };
        @vertex fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            out.clip_pos = u_transform * vec4f(in.position, 1.0);
            return out;
        }
        @fragment fn fs_main() -> @location(0) vec4f { return u_color; }
    )";

        WGPUShaderSourceWGSL wgslSrc = {};
        wgslSrc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgslSrc.code = {wgslSource, strlen(wgslSource)};

        WGPUShaderModuleDescriptor shaderDesc = {};
        shaderDesc.nextInChain = (const WGPUChainedStruct *)&wgslSrc;
        WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(g_device, &shaderDesc);

        WGPUBindGroupLayoutEntry bglEntries[2] = {};
        bglEntries[0].binding = 0;
        bglEntries[0].visibility = WGPUShaderStage_Fragment;
        bglEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
        bglEntries[1].binding = 1;
        bglEntries[1].visibility = WGPUShaderStage_Vertex;
        bglEntries[1].buffer.type = WGPUBufferBindingType_Uniform;

        WGPUBindGroupLayoutDescriptor bglDesc = {};
        bglDesc.entryCount = 2;
        bglDesc.entries = bglEntries;
        WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(g_device, &bglDesc);

        WGPUBindGroupEntry bgEntries[2] = {};
        bgEntries[0].binding = 0;
        bgEntries[0].buffer = g_colorBuffer;
        bgEntries[0].offset = 0;
        bgEntries[0].size = 16;
        bgEntries[1].binding = 1;
        bgEntries[1].buffer = g_transformBuffer;
        bgEntries[1].offset = 0;
        bgEntries[1].size = 64;

        WGPUBindGroupDescriptor bgDesc = {};
        bgDesc.layout = bindGroupLayout;
        bgDesc.entryCount = 2;
        bgDesc.entries = bgEntries;
        g_bindGroup = wgpuDeviceCreateBindGroup(g_device, &bgDesc);

        WGPUPipelineLayoutDescriptor plDesc = {};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = &bindGroupLayout;
        WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(g_device, &plDesc);

        WGPURenderPipelineDescriptor pipelineDesc = {};
        pipelineDesc.layout = pipelineLayout;

        WGPUVertexAttribute vertAttr = {};
        vertAttr.format = WGPUVertexFormat_Float32x3;
        vertAttr.offset = 0;
        vertAttr.shaderLocation = 0;

        WGPUVertexBufferLayout vertLayout = {};
        vertLayout.arrayStride = 3 * sizeof(float);
        vertLayout.stepMode = WGPUVertexStepMode_Vertex;
        vertLayout.attributeCount = 1;
        vertLayout.attributes = &vertAttr;

        pipelineDesc.vertex.module = shaderModule;
        pipelineDesc.vertex.entryPoint = {"vs_main", strlen("vs_main")};
        pipelineDesc.vertex.bufferCount = 1;
        pipelineDesc.vertex.buffers = &vertLayout;

        WGPUColorTargetState colorTarget = {};
        colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragmentState = {};
        fragmentState.module = shaderModule;
        fragmentState.entryPoint = {"fs_main", strlen("fs_main")};
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;
        pipelineDesc.fragment = &fragmentState;

        WGPUDepthStencilState depthStencil = {};
        depthStencil.format = WGPUTextureFormat_Depth24Plus;
        depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
        depthStencil.depthCompare = WGPUCompareFunction_Less;
        pipelineDesc.depthStencil = &depthStencil;

        pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pipelineDesc.multisample.count = 1;
        pipelineDesc.multisample.mask = ~0u;

        g_pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pipelineDesc);
        wgpuShaderModuleRelease(shaderModule);
    }

    MIST_API void mist_poll_events() { glfwPollEvents(); }
    MIST_API bool mist_window_should_close() { return glfwWindowShouldClose(g_window); }

    MIST_API void mist_set_mesh_color(float r, float g, float b, float a)
    {
        float data[4] = {r, g, b, a};
        wgpuQueueWriteBuffer(g_queue, g_colorBuffer, 0, data, 16);
    }

    MIST_API void mist_set_transform(const float *matrix)
    {
        wgpuQueueWriteBuffer(g_queue, g_transformBuffer, 0, matrix, 64);
    }

    MIST_API void mist_set_vertex_buffer(const float *vertices, int vertexCount)
    {
        if (g_vertexBuffer)
            wgpuBufferRelease(g_vertexBuffer);
        g_vertexCount = vertexCount;
        size_t size = (size_t)vertexCount * 3 * sizeof(float);
        WGPUBufferDescriptor desc = {};
        desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        desc.size = size;
        g_vertexBuffer = wgpuDeviceCreateBuffer(g_device, &desc);
        wgpuQueueWriteBuffer(g_queue, g_vertexBuffer, 0, vertices, size);
    }

    MIST_API void mist_swap_buffers()
    {
        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(g_surface, &surfaceTexture);
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
            return;

        WGPUTextureView nextTexture = wgpuTextureCreateView(surfaceTexture.texture, nullptr);
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);

        WGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.view = nextTexture;
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.1, 0.1, 0.12, 1.0};

        WGPURenderPassDepthStencilAttachment depthAttachment = {};
        depthAttachment.view = g_depthView;
        depthAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        depthAttachment.depthClearValue = 1.0f;

        WGPURenderPassDescriptor renderPassDesc = {};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;
        renderPassDesc.depthStencilAttachment = &depthAttachment;

        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
        wgpuRenderPassEncoderSetPipeline(renderPass, g_pipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, g_bindGroup, 0, nullptr);
        if (g_vertexBuffer)
        {
            wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, g_vertexBuffer, 0, (size_t)g_vertexCount * 3 * sizeof(float));
            wgpuRenderPassEncoderDraw(renderPass, g_vertexCount, 1, 0, 0);
        }
        wgpuRenderPassEncoderEnd(renderPass);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(g_queue, 1, &cmd);
        wgpuSurfacePresent(g_surface);

        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(nextTexture);
        wgpuTextureRelease(surfaceTexture.texture);
    }
}