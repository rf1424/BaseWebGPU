// #include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include "webgpu/webgpu.hpp" // C++ wrapper
#include "webgpu-utils.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

using namespace wgpu;

const char* shaderSource = R"(

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) color: vec3f
};
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var o : VertexOutput;

    let AR =  640.0 / 480.0;
    o.position = vec4f(in.position, 0.0, 1.0);
    o.position.x /= AR;
    o.color = in.color;
    return o;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}
)";

class Application {
public:

    bool Initialize();

    void Terminate();

    void MainLoop();

    bool IsRunning();


private:
    TextureView GetNextSurfaceTextureView();
    void InitializePipeline();
    RequiredLimits GetRequiredLimits(Adapter adapter) const;
    void InitializeBuffers();

private:
    GLFWwindow* window;
    Device device;
    Queue queue;
    Surface surface;
    std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle; // TODO
    RenderPipeline pipeline;
    TextureFormat surfaceFormat = TextureFormat::Undefined;

    Buffer vertexBuffer;
    Buffer indexBuffer;
    
    uint32_t indexCount;

};

int main() {
    Application app;

    if (!app.Initialize()) {
        return 1;
    }

#ifdef __EMSCRIPTEN__ // TODO for web come back
    // Equivalent of the main loop when using Emscripten:
    auto callback = [](void* arg) {
        //                   ^^^ 2. We get the address of the app in the callback.
        Application* pApp = reinterpret_cast<Application*>(arg);
        //                  ^^^^^^^^^^^^^^^^ 3. We force this address to be interpreted
        //                                      as a pointer to an Application object.
        pApp->MainLoop(); // 4. We can use the application object
        };
    emscripten_set_main_loop_arg(callback, &app, 0, true);
    //                                     ^^^^ 1. We pass the address of our application object.
#else 
    while (app.IsRunning()) {
        app.MainLoop();
    }
#endif

    app.Terminate();

    return 0;
}


// APPLICATION METHODS IMPLEMENT
bool Application::Initialize() {
    // WINDOW  ----------------------------------------------------------------------------------------------

    glfwInit();
    // hints
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

    // INSTANCE SETUP ----------------------------------------------------------------------------------------------
    // create a descriptor
    InstanceDescriptor desc = {};
    // desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    // Make sure the uncaptured error callback is called as soon as an error
    // occurs rather than at the next call to "wgpuDeviceTick".
    DawnTogglesDescriptor toggles;
    toggles.chain.next = nullptr;
    toggles.chain.sType = SType::DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;

    desc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

    // instance using this descriptor
    Instance instance = createInstance(desc);

    // We can check whether there is actually an instance created
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return 1;
    }
    std::cout << "WGPU instance: " << instance << std::endl;

    // ADAPTER SETUP -----------------------------------------------------------------------------------------------
    std::cout << "Requesting adapter..." << std::endl;

    RequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    // surface
    surface = glfwGetWGPUSurface(instance, window);
    adapterOpts.compatibleSurface = surface;
    Adapter adapter = instance.requestAdapter(adapterOpts);

    std::cout << "Got adapter: " << adapter << std::endl;
    // after adapter obtained
    instance.release();

    // DEVICE ----------------------------------------------------------------------------------------------
    std::cout << "Requesting device..." << std::endl;
    // create default device descriptor!!!
    DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device"; // anything works here, that's your call
    deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
    deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    // set reuired limits
    RequiredLimits requiredLimits = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits = &requiredLimits;
    // deviceDesc.deviceLostCallback = nullptr;
    // A function that is invoked whenever the device stops being available -> Deprecated? TODO
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
        };
    device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << device << std::endl;


    //// Uncaptured Error Callback??? for  breakpoints TODO
    uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback(
        [](ErrorType type, char const* message) {
            std::cout << "Uncaptured error: " << type;
            if (message) std::cout << " (" << message << ")";
            std::cout << std::endl;
        }
    );

    //QUEUE OPERATIONS ----------------------------------------------------------------------------------------------
    queue = device.getQueue();

    // SURFACE CONFIGURATION -----------------------------------------
    SurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    // size of texture for now
    config.width = 640;
    config.height = 480;
    // flag as to-be used for render output
    config.usage = TextureUsage::RenderAttachment;
    // use format of the surface (from adapter)
    surfaceFormat = surface.getPreferredFormat(adapter);
    config.format = surfaceFormat;

    // no view formats
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = device;
    // presentation: first in, first out
    config.presentMode = PresentMode::Fifo;
    // transparency 
    config.alphaMode = CompositeAlphaMode::Auto;

    surface.configure(config);

    // after getting device & surface config
    adapter.release();

    InitializePipeline();

    InitializeBuffers();

    return true;
}

void Application::Terminate() {
    indexBuffer.release();
    vertexBuffer.release();
    pipeline.release();
    surface.unconfigure();
    queue.release();
    surface.release();
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::MainLoop() {

    glfwPollEvents();

    // next target texture view
    TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    // encoder for render pass
    CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "render-pass encoder";
    CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

    // render pass descriptor
    RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;
    // color attachment (just 1 for now)
    RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView; // draw to targetview
    renderPassColorAttachment.resolveTarget = nullptr; // only for multi-sampling

    renderPassColorAttachment.loadOp = LoadOp::Clear; // operation before render pass
    renderPassColorAttachment.storeOp = StoreOp::Store; // operation after rendre pass
    renderPassColorAttachment.clearValue = Color{ 0.1, 0.1, 0.9, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    // don't use these for now
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    // get access to commands for rendering (pass the descriptor)
    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());
	renderPass.setIndexBuffer(indexBuffer, IndexFormat::Uint16, 0, indexBuffer.getSize());
    renderPass.drawIndexed(indexCount, 1, 0, 0, 0);

    renderPass.end();
    renderPass.release();

    // encode and submit render command
    CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = "Command buffer";
    CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    // std::cout << "Submitting render command..." << std::endl;
    queue.submit(1, &command);
    command.release();
    // std::cout << "Command render submitted." << std::endl;

    // release at end
    targetView.release();

#ifndef __EMSCRIPTEN__
    surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

TextureView Application::GetNextSurfaceTextureView() {
    // first get surface texture (more like a raw container)
    SurfaceTexture surfaceTexture;
    surface.getCurrentTexture(&surfaceTexture);
    // check success
    if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
        return nullptr;
    }

    Texture texture = surfaceTexture.texture;

    // second create view texture (config TODO)
    TextureViewDescriptor viewDescriptor;
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = texture.getFormat();
    viewDescriptor.dimension = TextureViewDimension::_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = TextureAspect::All;
    TextureView targetView = texture.createView(viewDescriptor);

    // dont need surface texture once we get texture view
#ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must not be manually released)
    wgpuTextureRelease(surfaceTexture.texture);
#endif // WEBGPU_BACKEND_WGPU

    return targetView;
}

void Application::InitializePipeline() {
    RenderPipelineDescriptor pipelineDesc;

    // create shader module
    ShaderModuleDescriptor shaderDesc; // main description
    ShaderModuleWGSLDescriptor shaderCodeDesc; // additional, chained description for WGSL
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor; // set to WGSL
    shaderDesc.nextInChain = &shaderCodeDesc.chain; // connect additional to main via CHAIN
    shaderCodeDesc.code = shaderSource;
    ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    if (!shaderModule) {
        std::cerr << "Shader module creation failed!" << std::endl;
        return;
    }
    if (surfaceFormat == TextureFormat::Undefined) {
        std::cerr << "Surface format is undefined!" << std::endl;
        return;
    }

    // 0. Vertex pipeline state
    VertexState vertexState;
    // vertexBufferLayout
    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.arrayStride = 5 * sizeof(float); // 2f pos + 3f colors / vertex
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;
    // position attribute
    VertexAttribute positionAttrib;
    positionAttrib.shaderLocation = 0;
    positionAttrib.format = VertexFormat::Float32x2;
    positionAttrib.offset = 0;
    //color attribute
    VertexAttribute colorAttrib;
    colorAttrib.shaderLocation = 1;
    colorAttrib.format = VertexFormat::Float32x3;
    colorAttrib.offset = 2 * sizeof(float); // after position

    // pass position & color attr to vertexBufferLayout
    std::vector<VertexAttribute> vertexAttributes(2);
    vertexAttributes[0] = positionAttrib;
    vertexAttributes[1] = colorAttrib;
    vertexBufferLayout.attributeCount = vertexAttributes.size();
    vertexBufferLayout.attributes = vertexAttributes.data();

    //// pass vertexBufferLayout to pipelineDesc
    vertexState.bufferCount = 1;
    vertexState.buffers = &vertexBufferLayout;

    // shader contains: shader module, entry point
    vertexState.module = shaderModule;
    vertexState.entryPoint = "vs_main";
    vertexState.constantCount = 0;
    vertexState.constants = nullptr;

    //pipelineDesc.vertex.module = shaderModule;
    //pipelineDesc.vertex.entryPoint = "vs_main";
    //pipelineDesc.vertex.constantCount = 0;
    //pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.vertex = vertexState;

    // 1. Primitive pipeline state (primitive assembly and rasterization)
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined; // consider vertices sequentially
    pipelineDesc.primitive.frontFace = FrontFace::CCW; // Front if vertices increment CCW
    pipelineDesc.primitive.cullMode = WGPUCullMode_None; // no CULL yet: TODO

    // 2. Fragment shader state
    FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    // configure blending stage
    BlendState blendState;
    // rgb = a_s * rgb_s + (1 - a_s) * rgb_d
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = surfaceFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    // 3. Stencil/depth state
    pipelineDesc.depthStencil = nullptr;

    // Multi-sample 
    pipelineDesc.multisample.count = 1; // off for now
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    // ask backend to figure out the layout itself by inspecting the shader
    pipelineDesc.layout = nullptr;

    pipeline = device.createRenderPipeline(pipelineDesc);

    shaderModule.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) const {

    SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    RequiredLimits requiredLimits = Default;
    requiredLimits.limits.maxVertexAttributes = 2;
    requiredLimits.limits.maxVertexBuffers = 1;
    requiredLimits.limits.maxBufferSize = 6 * 5 * sizeof(float);
    requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
    //requiredLimits.limits.maxInterStageShaderComponents = 3; // 3f for color, doesn't count built-in components like position

    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

    return requiredLimits;
}

void Application::InitializeBuffers() {
    std::vector<float> vertices = {
        // center
        0.0f,  0.0f, 0.0, 0.0, 1.0,
        // top
         0.2f,  0.4f, 1.0, 0.0, 0.0,
        -0.2f,  0.4f, 1.0, 0.0, 0.0,
        // right
         0.4f,  0.2f, 0.0, 1.0, 0.0,
         0.4f, -0.2f, 0.0, 1.0, 0.0,
        // bottom
         -0.2f, -0.4f, 0.0, 1.0, 0.0,
          0.2f, -0.4f, 1.0, 0.0, 0.0,
        // left
         -0.4f, -0.2f, 1.0, 0.5, 1.0,
         -0.4f,  0.2f, 0.5, 1.0, 1.0,
    };

	std::vector<uint16_t> indices = {
		0, 1, 2, // top triangle
		0, 3, 4, // right triangle
		0, 5, 6, // bottom triangle
		0, 7, 8, // left triangle
	};
	indices.resize((indices.size() + 1) & ~1); // align to 2 bytes for index buffer

    BufferDescriptor indexBufferDesc;
	indexBufferDesc.label = "Index Buffer";
	indexBufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
	indexBufferDesc.size = indices.size() * sizeof(uint16_t);
	indexBufferDesc.size = (indexBufferDesc.size + 3) & ~3; // align to 4 bytes
    indexBufferDesc.mappedAtCreation = false;
    indexBuffer = device.createBuffer(indexBufferDesc);
	queue.writeBuffer(indexBuffer, 0, indices.data(), indexBufferDesc.size);

	indexCount = static_cast<uint32_t>(indices.size());

    BufferDescriptor bufferDesc;
    bufferDesc.label = "Vertex Buffer";
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.size = vertices.size() * sizeof(float);
	bufferDesc.size = (bufferDesc.size + 3) & ~3; // align to 4 bytes
    bufferDesc.mappedAtCreation = false;

    vertexBuffer = device.createBuffer(bufferDesc);

    queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));
}



