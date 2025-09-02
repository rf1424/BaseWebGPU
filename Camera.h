#pragma once

#include <glm/ext.hpp>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


struct DragState {
	bool dragging = false;
	glm::vec2 startMousePos = glm::vec2(0.);
	glm::vec2 startCameraAngles = glm::vec2(0.);
};

class Camera
{

public:


	// interaction state
	DragState dragState;

	
	// camera state
	glm::vec2 angles = { 0.0f, 0.0f }; // in radians
	float zoom = -0.5f;
	

public:
	
	void getViewMatrix(glm::mat4& outViewMatrix);
};
