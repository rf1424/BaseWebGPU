// #include <webgpu/webgpu.h>
#define WEBGPU_CPP_IMPLEMENTATION
#include "webgpu/webgpu.hpp" // C++ wrapper
#include "webgpu-utils.h"
#include "FileManagement.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <array>
#include <glm/ext.hpp>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

using namespace wgpu;

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
    void InitializeBindGroups();
    void InitializeDepthTexture();

private:
    GLFWwindow* window;
    Device device;
    Queue queue;
    Surface surface;
    std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle; // TODO
    RenderPipeline pipeline;
    TextureFormat surfaceFormat = TextureFormat::Undefined;

    // uniform bindings
    BindGroupLayout bindGroupLayout;
    BindGroup bindGroup;
    PipelineLayout layout;


    // buffers
    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer uniformBuffer;

    struct Uniforms {
        glm::mat4x4 projMatrix; // alignment: 16
        glm::mat4x4 viewMatrix;
        glm::mat4x4 modelMatrix;
        float time;
        // padding
        float padding[3]; // time + pad = 16 bytes for alignment!
    };

    struct VertexAttr {
        glm::vec3 position;
        glm::vec3 color;
        glm::vec3 normal;
        glm::vec2 uv;
    };
    
    uint32_t indexCount = 0;

    //depth setup
    Texture depthTexture;
    TextureView depthTextureView;
    TextureFormat depthTextureFormat = TextureFormat::Undefined;

    
    
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

    depthTextureFormat = TextureFormat::Depth24Plus;
    InitializePipeline();

    InitializeBuffers();
    InitializeDepthTexture();
    InitializeBindGroups(); // after buffers are created and passed

    return true;
}

void Application::Terminate() {
    

    indexBuffer.release();
    vertexBuffer.release();
    uniformBuffer.release();
    layout.release();
    bindGroupLayout.release();
    bindGroup.release();
    pipeline.release();

    depthTextureView.release();
    depthTexture.destroy();
    depthTexture.release();

    surface.unconfigure();
    queue.release();
    surface.release();
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::MainLoop() {

    glfwPollEvents();

    //update uniforms
    float t = static_cast<float>(glfwGetTime()); 
    queue.writeBuffer(uniformBuffer, sizeof(glm::mat4x4) * 3., &t, sizeof(float)); // offset for mvp
    glm::mat4x4 model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    glm::mat4x4 model2 = glm::rotate(glm::mat4(1.0f), (t), glm::vec3(1, 0, 0));
    model *= model2;
    queue.writeBuffer(uniformBuffer, offsetof(Uniforms, modelMatrix), &model, sizeof(glm::mat4x4));

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
    // renderPassColorAttachment.clearValue = Color{ 1., 0., 0., 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    // for depth buffer
    RenderPassDepthStencilAttachment depthStencilAttachment; 
	depthStencilAttachment.view = depthTextureView; // created in InitializeDepthTexture()
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = StoreOp::Store;
    /*depthStencilAttachment.depthLoadOp = LoadOp::Undefined;
    depthStencilAttachment.depthStoreOp = StoreOp::Undefined;*/
    depthStencilAttachment.depthReadOnly = false;
    // do not use stencil for now (but setup required)
    /*depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = StoreOp::Store;
    depthStencilAttachment.stencilReadOnly = true;*/
	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    // TODO: necessary for DAWN maybe?
    /*constexpr auto NaNf = std::numeric_limits<float>::quiet_NaN();
    depthStencilAttachment.clearDepth = NaNf;*/

    renderPassDesc.timestampWrites = nullptr;

    // get access to commands for rendering (pass the descriptor)
    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());
	renderPass.setIndexBuffer(indexBuffer, IndexFormat::Uint16, 0, indexBuffer.getSize());
    renderPass.setBindGroup(0, bindGroup, 0, nullptr);
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

    //// create shader module
    //ShaderModuleDescriptor shaderDesc; // main description
    //ShaderModuleWGSLDescriptor shaderCodeDesc; // additional, chained description for WGSL
    //shaderCodeDesc.chain.next = nullptr;
    //shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor; // set to WGSL
    //shaderDesc.nextInChain = &shaderCodeDesc.chain; // connect additional to main via CHAIN
    //shaderCodeDesc.code = shaderSource;
    //ShaderModule shaderModule = device.createShaderModule(shaderDesc);
	ShaderModule shaderModule = FileManagement::loadShaderModule("../files/shader0.wgsl", device);

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
    vertexBufferLayout.arrayStride = sizeof(VertexAttr);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;
    // position attribute
    VertexAttribute positionAttrib;
    positionAttrib.shaderLocation = 0;
    positionAttrib.format = VertexFormat::Float32x3;
    positionAttrib.offset = 0;
    //color attribute
    VertexAttribute colorAttrib;
    colorAttrib.shaderLocation = 1;
    colorAttrib.format = VertexFormat::Float32x3;
    colorAttrib.offset = offsetof(VertexAttr, color);

    //normal attribute
    VertexAttribute normalAttrib;
    normalAttrib.shaderLocation = 2;
    normalAttrib.format = VertexFormat::Float32x3; //vec3
    normalAttrib.offset = offsetof(VertexAttr, normal);
    // uv attribute
    VertexAttribute uvAttrib;
    uvAttrib.shaderLocation = 3;
    uvAttrib.format = VertexFormat::Float32x2;
    uvAttrib.offset = offsetof(VertexAttr, uv);

    // pass position & color attr to vertexBufferLayout
    std::vector<VertexAttribute> vertexAttributes(4);
    vertexAttributes[0] = positionAttrib;
    vertexAttributes[1] = colorAttrib;
    vertexAttributes[2] = normalAttrib;
	vertexAttributes[3] = uvAttrib;
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
    DepthStencilState depthStencilState = Default;
	depthStencilState.depthCompare = CompareFunction::LessEqual;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = depthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    pipelineDesc.depthStencil = &depthStencilState;


    // Multi-sample 
    pipelineDesc.multisample.count = 1; // off for now
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;


    // define pipeline layout (describe pipeline resources)
    // Binding Layout
    BindGroupLayoutEntry bindingLayout = Default;// 0. sets buffer, sampler, etc. to undefined
    bindingLayout.binding = 0;// binding index, same as attrubute used in shader for uTime
    bindingLayout.visibility = ShaderStage::Vertex;
    bindingLayout.buffer.type = BufferBindingType::Uniform; // 1. undefined -> BUFFER
    bindingLayout.buffer.minBindingSize = sizeof(Uniforms);
    // Bindinh group of binding layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindingLayout;
    bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);
    // layout descriptor
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    layout = device.createPipelineLayout(layoutDesc);

    // ask backend to figure out the layout itself by inspecting the shader
    pipelineDesc.layout = layout;

    pipeline = device.createRenderPipeline(pipelineDesc);

    shaderModule.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) const {

    SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    RequiredLimits requiredLimits = Default;
    requiredLimits.limits.maxVertexAttributes = 2;
    requiredLimits.limits.maxVertexBuffers = 1;
    //requiredLimits.limits.maxBufferSize = 6 * 5 * sizeof(float);
    //requiredLimits.limits.maxVertexBufferArrayStride = 6 * sizeof(float);
    //requiredLimits.limits.maxInterStageShaderComponents = 3; // 3f for color, doesn't count built-in components like position

    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

    // depth texture
    requiredLimits.limits.maxTextureDimension1D = 480;
    requiredLimits.limits.maxTextureDimension2D = 640;
    requiredLimits.limits.maxTextureArrayLayers = 1;

    // for uniforms
    requiredLimits.limits.maxBindGroups = 1;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxUniformBufferBindingSize = sizeof(Uniforms);

    return requiredLimits;
}

void Application::InitializeBuffers() {
	std::vector<float> vertices; // vertex data
	std::vector<uint16_t> indices; // index data
    //bool success = FileManagement::loadGeometry("../files/sampleGeo.txt", vertices, indices, 2);
    bool success = FileManagement::loadGeometry("../files/pyramidGeo.txt", vertices, indices, 8); // TODO temp: 8 for pos nor uv
    if (!success) {
        std::cerr << "Could not load geometry!" << std::endl;
        exit(1);
    }

    indices.resize((indices.size() + 1) & ~1); // align to 2 bytes for index buffer
    indexCount = static_cast<uint32_t>(indices.size());

    // INDEX BUFFER
    BufferDescriptor indexBufferDesc;
    indexBufferDesc.label = "Index Buffer";
    indexBufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
    indexBufferDesc.size = indices.size() * sizeof(uint16_t);
    indexBufferDesc.size = (indexBufferDesc.size + 3) & ~3; // align to 4 bytes
    indexBufferDesc.mappedAtCreation = false;

    indexBuffer = device.createBuffer(indexBufferDesc);
    queue.writeBuffer(indexBuffer, 0, indices.data(), indexBufferDesc.size);

    // VERTEX BUFFER
    BufferDescriptor bufferDesc;
    bufferDesc.label = "Vertex Buffer";
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.size = vertices.size() * sizeof(float);
    bufferDesc.size = (bufferDesc.size + 3) & ~3; // align to 4 bytes
    bufferDesc.mappedAtCreation = false;

    vertexBuffer = device.createBuffer(bufferDesc);
    queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

    // UNIFORM BUFFER
    BufferDescriptor uniformBufferDesc;
    uniformBufferDesc.label = "Uniform Buffer";
    uniformBufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.size = (uniformBufferDesc.size + 3) & ~3; // align to 4 bytes
    uniformBufferDesc.mappedAtCreation = false;

	uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    Uniforms uniforms;
    uniforms.time = 1.0f;

    uniforms.viewMatrix = glm::lookAt(
        glm::vec3(0, 0, 3),
        glm::vec3(0, 0, 0),
        glm::vec3(0, 1, 0)
    );
    uniforms.projMatrix = glm::perspective(glm::radians(45.0f), 640.0f / 480.0f, 0.1f, 100.0f);
    uniforms.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(Uniforms));
}


void Application::InitializeBindGroups() {
    BindGroupEntry binding{};
    binding.binding = 0;
    binding.buffer = uniformBuffer;
    binding.offset = 0;
    binding.size = sizeof(Uniforms);

    BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout; // defined in layer pipeline
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &binding;
    bindGroup = device.createBindGroup(bindGroupDesc);
}

void Application::InitializeDepthTexture()
{
    // depth texture
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = { 640, 480, 1 };
    depthTextureDesc.usage = TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
    depthTexture = device.createTexture(depthTextureDesc);
    
    // texture view for accessibility
    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = depthTextureFormat;
    depthTextureView = depthTexture.createView(depthTextureViewDesc);
}
