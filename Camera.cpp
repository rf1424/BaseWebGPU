#include "Camera.h"

// camera position in Cartesian coords from angles and r (z is up here)
void Camera::getViewMatrix(glm::mat4& outViewMatrix) {
	float cx = glm::cos(angles.x);
	float sx = sin(angles.x);
	float cy = cos(angles.y);
	float sy = sin(angles.y);
	glm::vec3 position = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-zoom);
	glm::mat4 viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0, 0, 1));
	outViewMatrix = viewMatrix;
}