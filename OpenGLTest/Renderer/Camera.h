#pragma once

#include "Transform.h"

class Camera
{
public:
	Camera();
	Camera(float fieldOfView, unsigned int width, unsigned int height, float nearClip, float farClip);

	glm::mat4 getViewMatrix();
	glm::mat4 getProjectionMatrix();

	Transform transform;
private:
	float fieldOfView;
	unsigned int width;
	unsigned int height;
	float nearClip;
	float farClip;
};