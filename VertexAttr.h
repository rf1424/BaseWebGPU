#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Simple struct to hold vertex attributes
struct VertexAttr {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
}; 