#pragma once
#include "webgpu/webgpu.hpp" 
#include "VertexAttr.h"
#include "Camera.h"

#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <vector>
#include <filesystem>

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
        glm::mat4x4 modelInvTranspose;
        float time;
        float padding[3]; // padding: time + pad = 16 bytes for alignment!
        glm::vec3 cameraPos;
		float padding1; // cameraPos + pad1 = 16 bytes for alignment!
    };

    uint32_t indexCount = 0;

    //depth setup
    Texture depthTexture;
    TextureView depthTextureView;
    TextureFormat depthTextureFormat = TextureFormat::Undefined;

    TextureView colorTextureView; // TODO: textureformat?
    Sampler sampler;

    Texture cubemap;
	TextureView cubemapTextureView;

    Camera viewCamera;


private:
    TextureView GetNextSurfaceTextureView();
    void InitializePipeline();
    RequiredLimits GetRequiredLimits(Adapter adapter) const;
    void InitializeSurface();
    void InitializeBuffers();
    void InitializeBindGroups();
    void InitializeDepthTexture();
    Texture InitializeCubeMapTexture(const std::filesystem::path& basePath, TextureView* textureView = nullptr);
    Texture getObjTexture(const std::filesystem::path& path, Device device, TextureView* textureView = nullptr);

    void reSizeScreen();
    // camera methods
    void updateViewMatrix();
    void onClick(int button, int action, int);
    void onDrag(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
};