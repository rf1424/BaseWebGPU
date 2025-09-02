#define WEBGPU_CPP_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION // Add to one .cpp file
#define STB_IMAGE_IMPLEMENTATION

#include "webgpu/webgpu.hpp"    // C++ wrapper
#include "webgpu-utils.h"

#include "FileManagement.h"
#include "VertexAttr.h"
#include "stb_image.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <array>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <glm/ext.hpp>

#include "Camera.h"



// Emscripten
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
    GLFWwindow* window;
    Device device;
    Adapter adapter;
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
    // Buffer indexBuffer;
    Buffer uniformBuffer;

    struct Uniforms {
        glm::mat4x4 projMatrix; // alignment: 16
        glm::mat4x4 viewMatrix;
        glm::mat4x4 modelMatrix;
        float time;
        // padding
        float padding[3]; // time + pad = 16 bytes for alignment!
    };

    uint32_t indexCount = 0;

    //depth setup
    Texture depthTexture;
    TextureView depthTextureView;
    TextureFormat depthTextureFormat = TextureFormat::Undefined;


    TextureView colorTextureView; // TODO: how about textureformat?
    Sampler sampler;

    Camera viewCamera;
private:
    TextureView GetNextSurfaceTextureView();
    void InitializePipeline();
    RequiredLimits GetRequiredLimits(Adapter adapter) const;
    void InitializeSurface();
    void InitializeBuffers();
    void InitializeBindGroups();
    void InitializeDepthTexture();
    Texture getObjTexture(const std::filesystem::path& path, Device device, TextureView* textureView = nullptr);

    // camera stuff
    void reSizeScreen();


    void updateViewMatrix();

    void onClick(int button, int action, int);
    void onDrag(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
};


void Application::updateViewMatrix() {
    // call camera's view matrix function
    glm::mat4x4 viewMatrix;
    viewCamera.getViewMatrix(viewMatrix);
    // send to shader
    queue.writeBuffer(
        uniformBuffer,
        offsetof(Uniforms, viewMatrix),
        &viewMatrix,
        sizeof(Uniforms::viewMatrix)
    );
}


void Application::onClick(int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (action) {
        case GLFW_PRESS:
            // start dragging
            viewCamera.dragState.dragging = true;
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            viewCamera.dragState.startMousePos = glm::vec2(-(float)xpos, (float)ypos); // TODO - ?
            viewCamera.dragState.startCameraAngles = viewCamera.angles;
            break;

        case GLFW_RELEASE:
            // stop dragging
            viewCamera.dragState.dragging = false;
            break;
        }
    }
}

void Application::onDrag(double xpos, double ypos) {
    if (!viewCamera.dragState.dragging) return;
    glm::vec2 currMousePos = glm::vec2(-(float)xpos, (float)ypos);
    glm::vec2 offset = currMousePos - viewCamera.dragState.startMousePos;
    viewCamera.angles = viewCamera.dragState.startCameraAngles + offset * 0.01f;
    viewCamera.angles.y = glm::clamp(
        viewCamera.angles.y,
        -3.14159f / 2 + 1e-5f,
        3.14159f / 2 - 1e-5f
    );
    updateViewMatrix();
}

void Application::onScroll(double xoffset, double yoffset) {
    viewCamera.zoom += yoffset * 0.1f;
    viewCamera.zoom = glm::clamp(viewCamera.zoom, -2.0f, 2.0f);
    updateViewMatrix();
}

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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(640, 480, "WebGPU", nullptr, nullptr);

    // glfw window -> Application instance
    // set user pointer to access application instance in callbacks
    glfwSetWindowUserPointer(window, this);

    // create lambda function for resize callback using pointer to App
    // must be a NON-Capturing lambda to be converted to function pointer
    auto resizeCallback = [](GLFWwindow* window, int width, int height) {
        // get application instance
        Application* appPtr = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (appPtr) appPtr->reSizeScreen();
        };
    // finally set the callback!
    glfwSetFramebufferSizeCallback(window, resizeCallback);

    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
        // get application instance
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onDrag(xpos, ypos);
        });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        // get application instance
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onClick(button, action, mods);
        });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
        // get application instance
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onScroll(xoffset, yoffset);
        });


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
    adapter = instance.requestAdapter(adapterOpts);

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



    uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback(
        [](ErrorType type, char const* message) {
            std::cout << "Uncaptured error callback: " << type;
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
     //adapter.release();

    depthTextureFormat = TextureFormat::Depth24Plus;
    InitializePipeline();

    InitializeBuffers();
    InitializeDepthTexture();

    Texture colorTexture = getObjTexture("../files/Lidded_Ewer.jpeg", device, &colorTextureView);
    if (!colorTexture) {
        std::cerr << "Could not load texture!" << std::endl;
    }

    InitializeBindGroups(); // after buffers are created and passed

    return true;
}

void Application::Terminate() {


    // indexBuffer.release();
    vertexBuffer.release();
    uniformBuffer.release();
    layout.release();
    bindGroupLayout.release();
    bindGroup.release();
    pipeline.release();

    depthTextureView.release();
    depthTexture.destroy();
    depthTexture.release();

    colorTextureView.release();

    adapter.release();
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
    //glm::mat4x4 model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    ////glm::mat4x4 model2 = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1, 0, 0));
    //glm::mat4x4 model2 = glm::rotate(glm::mat4(1.0f), t, glm::vec3(1, 0, 0));
    //model *= model2;

    glm::mat4x4 model = glm::rotate(glm::mat4(1.0f), t, glm::vec3(0, 1, 0));

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
    // renderPass.setIndexBuffer(indexBuffer, IndexFormat::Uint16, 0, indexBuffer.getSize());
    renderPass.setBindGroup(0, bindGroup, 0, nullptr);
    // renderPass.drawIndexed(indexCount, 1, 0, 0, 0);
    renderPass.draw(indexCount, 1, 0, 0);
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
    // Uniforms Binding Layout
    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(3, Default); // Default sets buffer, sampler, etc. to undefined

    // 0. Uniforms
    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;// binding index, same as attrubute used in shader for uTime
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform; // 1. undefined -> BUFFER
    bindingLayout.buffer.minBindingSize = sizeof(Uniforms);

    // 1. Texture Binding Layout
    BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
    textureBindingLayout.binding = 1;
    textureBindingLayout.visibility = ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType = TextureSampleType::Float;
    textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    // 2. Sampler
    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
    samplerBindingLayout.binding = 2;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

    // Binding group of binding layout
    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
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
    requiredLimits.limits.maxVertexAttributes = 4; // pos col nor uv
    requiredLimits.limits.maxVertexBuffers = 1;
    // requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttr);
    // requiredLimits.limits.maxVertexBufferArrayStride = 6 * sizeof(float);
    // requiredLimits.limits.maxInterStageShaderComponents = 3; // 3f for color, doesn't count built-in components like position

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

    // textures
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
    requiredLimits.limits.maxSamplersPerShaderStage = 1;

    requiredLimits.limits.maxTextureDimension1D = 2048;
    requiredLimits.limits.maxTextureDimension2D = 2048;

    return requiredLimits;
}

void Application::InitializeSurface()
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    SurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    // size of texture for now
    config.width = width;
    config.height = height;
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
}

void Application::InitializeBuffers() {


    std::vector<VertexAttr> verticesList;
    bool success = FileManagement::getObjGeometry("../files/Lidded_Ewer.obj", verticesList);
    if (!success) {
        std::cerr << "Could not load geometry!" << std::endl;
        exit(1);
    }

    indexCount = static_cast<uint32_t>(verticesList.size());

    // VERTEX BUFFER
    BufferDescriptor bufferDesc;
    bufferDesc.label = "Vertex Buffer";
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.size = verticesList.size() * sizeof(VertexAttr);
    bufferDesc.size = (bufferDesc.size + 3) & ~3; // align to 4 bytes
    bufferDesc.mappedAtCreation = false;

    vertexBuffer = device.createBuffer(bufferDesc);
    queue.writeBuffer(vertexBuffer, 0, verticesList.data(), bufferDesc.size);

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

    viewCamera.getViewMatrix(uniforms.viewMatrix);
    viewCamera.getProjMatrix(uniforms.projMatrix);
    /*uniforms.viewMatrix = glm::lookAt(
        glm::vec3(0, 0.2, 0.5),
        glm::vec3(0, 0.15, 0),
        glm::vec3(0, 1, 0)
    );*/
    //uniforms.projMatrix = glm::perspective(glm::radians(45.0f), 640.0f / 480.0f, 0.1f, 1000.0f);
    uniforms.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));

    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(Uniforms));
}


void Application::InitializeBindGroups() {
    // UNIFORM
    BindGroupEntry binding{};
    binding.binding = 0;
    binding.buffer = uniformBuffer;
    binding.offset = 0;
    binding.size = sizeof(Uniforms);

    // TEXTURE
    BindGroupEntry textureBinding{}; // TODO: other specidications?????
    textureBinding.binding = 1;
    textureBinding.textureView = colorTextureView;

    // SAMPLER
    BindGroupEntry samplerBinding{};
    samplerBinding.binding = 2;
    samplerBinding.sampler = sampler;


    std::vector<BindGroupEntry> bindingEntries(3);
    bindingEntries[0] = binding;
    bindingEntries[1] = textureBinding;
    bindingEntries[2] = samplerBinding;
    BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout; // defined in layer pipeline
    bindGroupDesc.entryCount = (uint32_t)bindingEntries.size();
    bindGroupDesc.entries = bindingEntries.data();
    bindGroup = device.createBindGroup(bindGroupDesc);
}

void Application::InitializeDepthTexture()
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // depth texture
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
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


    SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = AddressMode::ClampToEdge;
    samplerDesc.addressModeV = AddressMode::ClampToEdge;
    samplerDesc.addressModeW = AddressMode::ClampToEdge;
    samplerDesc.magFilter = FilterMode::Linear;
    samplerDesc.minFilter = FilterMode::Linear;
    samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1.0f;
    samplerDesc.compare = CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    sampler = device.createSampler(samplerDesc);
}

Texture Application::getObjTexture(const std::filesystem::path& path, Device device, TextureView* textureView)
{

    int width, height, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4); // 4 rgba

    if (nullptr == data) return nullptr;


    // create texture descriptor
    TextureDescriptor textureDesc;
    textureDesc.dimension = TextureDimension::_2D;
    textureDesc.format = WGPUTextureFormat_RGBA8Unorm; // unsigned, normalized 0-1
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst; // shader binding & copy from CPU
    textureDesc.viewFormatCount = 0; // no alternate formats for texture view
    textureDesc.viewFormats = nullptr;

    Texture colorTexture = device.createTexture(textureDesc);



    ImageCopyTexture destination;
    destination.texture = colorTexture;
    destination.mipLevel = 0;
    destination.origin = { 0, 0, 0 };
    destination.aspect = TextureAspect::All;
    TextureDataLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * textureDesc.size.width;
    source.rowsPerImage = textureDesc.size.height;

    queue.writeTexture(destination, data, width * height * 4, source, textureDesc.size);
    // TODO mipmapping


    // LOADING MY OWN TEXTURE INSTEAD -------------------------------------------------------
    //std::vector<uint8_t> pixels(4 * textureDesc.size.width * textureDesc.size.height);
    //int cellSize = 32;
    //for (uint32_t i = 0; i < textureDesc.size.width; ++i) {
    //    for (uint32_t j = 0; j < textureDesc.size.height; ++j) {
    //        uint8_t* p = &pixels[4 * (j * textureDesc.size.width + i)];

    //        float dx = i - textureDesc.size.width / 2.0f;
    //        float dy = j - textureDesc.size.height / 2.0f;
    //        int band = static_cast<int>(sqrtf(dx * dx + dy * dy) / 4) % 2; // ring size = 4px

    //        uint8_t v = band ? 255 : 0; // alternate black/white
    //        p[0] = v;
    //        p[1] = v;
    //        p[2] = v;
    //        p[3] = 255;
    //    }
    //}
    // queue.writeTexture(destination, pixels.data(), pixels.size(), source, textureDesc.size);
    // ----------------------------------------------------------------------------------------------

    // release
    stbi_image_free(data);

    if (textureView) {
        // texture view
        TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = 1;
        textureViewDesc.dimension = TextureViewDimension::_2D;
        textureViewDesc.format = textureDesc.format;

        *textureView = colorTexture.createView(textureViewDesc);
    }


    return colorTexture;
}

void Application::reSizeScreen()
{
    // terminate depth texture & surface
    depthTextureView.release();
    depthTexture.destroy();
    depthTexture.release();
    surface.unconfigure();
    // surface.release(); DONT

    InitializeDepthTexture();
    InitializeSurface();


}
