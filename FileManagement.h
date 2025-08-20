#pragma once
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <webgpu/webgpu.hpp>
#include "VertexAttr.h"
#include "tiny_obj_loader.h"

class FileManagement
{
public:

    static bool getObjGeometry(const std::filesystem::path& path, std::vector<VertexAttr>& vertexData);

    static bool loadGeometry(
        const std::filesystem::path& path,
        std::vector<float>& pointData,
        std::vector<uint16_t>& indexData,
		int dimensions
    );


    static wgpu::ShaderModule loadShaderModule(
        const std::filesystem::path& filepath,
        wgpu::Device device
    );
private:

};

