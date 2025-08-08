#pragma once
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <webgpu/webgpu.hpp>

class FileManagement
{
public:
    static bool loadGeometry(
        const std::filesystem::path& path,
        std::vector<float>& pointData,
        std::vector<uint16_t>& indexData
    );

    static wgpu::ShaderModule loadShaderModule(
        const std::filesystem::path& filepath,
        wgpu::Device device
    );
private:

};

