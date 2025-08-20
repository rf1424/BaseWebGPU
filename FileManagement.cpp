#include "FileManagement.h"



bool FileManagement::getObjGeometry(const std::filesystem::path& path, std::vector<VertexAttr>& vertexData)
{
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = path.parent_path().string(); // use directory of input path

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path.string(), reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error() << std::endl;
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning() << std::endl;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    vertexData.clear();

    for (const auto& shape : shapes) {
        size_t offset = vertexData.size();
        vertexData.resize(offset + shape.mesh.indices.size()); // for next shape

        for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
            const tinyobj::index_t& idx = shape.mesh.indices[i];
            VertexAttr v{};

            // pos + color
            if (idx.vertex_index >= 0) {
                v.position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };

                if (!attrib.colors.empty()) {
                    v.color = {
                        attrib.colors[3 * idx.vertex_index + 0],
                        attrib.colors[3 * idx.vertex_index + 1],
                        attrib.colors[3 * idx.vertex_index + 2]
                    };
                }
                else {
                    v.color = glm::vec3(1.0f); // default white if no color data
                }
            }

            // normal
            if (idx.normal_index >= 0) {
                v.normal = {
                    attrib.normals[3 * idx.normal_index + 0],
                    attrib.normals[3 * idx.normal_index + 1],
                    attrib.normals[3 * idx.normal_index + 2]
                };
            }

            // uv
            if (idx.texcoord_index >= 0) {
                v.uv = {
                    attrib.texcoords[2 * idx.texcoord_index + 0],
                    1 - attrib.texcoords[2 * idx.texcoord_index + 1] // y-axis: 0-1 up for obj -> 0 to 1 down for webgpu
                };
            }
            vertexData[offset + i] = v;
        }
    }

    return true;
}

// loadGeometry code method from WEbGPU C++ Guide
bool FileManagement::loadGeometry(
    const std::filesystem::path& path,
    std::vector<float>& pointData,
    std::vector<uint16_t>& indexData,
	int dimensions
) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    pointData.clear();
    indexData.clear();

    enum class Section {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof()) {
        getline(file, line);

        // overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line == "[points]") {
            currentSection = Section::Points;
        }
        else if (line == "[indices]") {
            currentSection = Section::Indices;
        }
        else if (line[0] == '#' || line.empty()) {
            // Do nothing, this is a comment
        }
        else if (currentSection == Section::Points) {
            std::istringstream iss(line);
            // Get x, y, z, r, g, b
            for (int i = 0; i < 3 + dimensions; ++i) {
                iss >> value;
                pointData.push_back(value);
            }
        }
        else if (currentSection == Section::Indices) {
            std::istringstream iss(line);
            // Get corners #0 #1 and #2
            for (int i = 0; i < 3; ++i) {
                iss >> index;
                indexData.push_back(index);
            }
        }
    }
    return true;
}





wgpu::ShaderModule FileManagement::loadShaderModule(const std::filesystem::path& filepath,
                                                    wgpu::Device device) {

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);


    // create shader module
    wgpu::ShaderModuleDescriptor shaderDesc; // main description
    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc; // additional, chained description for WGSL
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor; // set to WGSL

    shaderDesc.nextInChain = &shaderCodeDesc.chain; // connect additional to main via CHAIN
    shaderCodeDesc.code = shaderSource.c_str();
    return device.createShaderModule(shaderDesc);
}

