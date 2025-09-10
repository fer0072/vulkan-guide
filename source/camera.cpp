#include "camera.h"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include "vk_types.h"

glm::mat4 Camera::getViewMatrix() const
{
	glm::vec3 camPos = position;

	glm::mat4 camRot = (getRotationMatrix());

	glm::mat4 view = glm::translate(glm::mat4{ 1 }, camPos) * camRot;

	//we need to invert the camera matrix
	view = glm::inverse(view);

	return view;
}

glm::mat4 Camera::getProjectionMatrix() const
{
	glm::mat4 pro = glm::perspective(glm::radians(70.f), 1700.f / 900.f, vkGlobals::g_zFar, vkGlobals::g_zNear);
	pro[1][1] *= -1;
	return pro;
}

glm::mat4 Camera::getRotationMatrix() const
{
	glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3{ 0.f, 1.f, 0.f });
	glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3{ 1.f, 0.f, 0.f });
	
	return yawRotation * pitchRotation;
}

void Camera::processInputEvent(SDL_Event* ev)
{
	if (ev->type == SDL_KEYDOWN)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_w:
			inputAxis.z -= 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_s:
			inputAxis.z += 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_a:
			inputAxis.x -= 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			inputAxis.x += 1.f;
			break;
		case SDLK_q:
			inputAxis.y -= 1.f;
			break;

		case SDLK_e:
			inputAxis.y += 1.f;
			break;
		case SDLK_LSHIFT:
			bSprint = true;
			break;
		}
	}
	else if (ev->type == SDL_KEYUP)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_w:
			inputAxis.z += 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_s:
			inputAxis.z -= 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_a:
			inputAxis.x += 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			inputAxis.x -= 1.f;
			break;
		case SDLK_q:
			inputAxis.y += 1.f;
			break;
		case SDLK_e:
			inputAxis.y -= 1.f;
			break;
		case SDLK_LSHIFT:
			bSprint = false;
			break;
		}
	}
	else if (ev->type == SDL_MOUSEMOTION) {
		if (!bLocked)
		{
			yaw -= ev->motion.xrel * 0.003f;
			pitch -= ev->motion.yrel * 0.003f;
		}
	}

	inputAxis = glm::clamp(inputAxis, { -1.0,-1.0,-1.0 }, { 1.0,1.0,1.0 });
}

void Camera::updateCamera(float deltaSeconds)
{
	float cameraVelocity = 0.001f + bSprint * 0.01f;

	glm::mat4 cameraRotation = getRotationMatrix();

	velocity = cameraRotation * glm::vec4(inputAxis, 0.0f) * cameraVelocity;

	position += (velocity * 10.0f * deltaSeconds);
}

