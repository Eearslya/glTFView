#pragma once

#include <Tsuki/Input.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

enum class CameraType { Orbit, FirstPerson };

class Camera {
 public:
	void Rotate(const glm::vec3& dRot) {
		Rotation += dRot;
		UpdateView();
	}
	void SetAspectRatio(float aspect) {
		SetPerspective(_fovDegrees, aspect, _zNear, _zFar);
	}
	void SetPerspective(float fovDegrees, float aspect, float zNear, float zFar) {
		_fovDegrees = fovDegrees;
		_zNear      = zNear;
		_zFar       = zFar;
		Perspective = glm::perspective(glm::radians(_fovDegrees), aspect, _zNear, _zFar);
	}
	void SetPosition(const glm::vec3& pos) {
		Position = pos;
		UpdateView();
	}
	void SetRotation(const glm::vec3& rot) {
		Rotation = rot;
		UpdateView();
	}
	void Translate(const glm::vec3& dPos) {
		Position += dPos;
		UpdateView();
	}

	CameraType Type       = CameraType::Orbit;
	glm::vec3 Position    = glm::vec3(0.0f);
	glm::vec3 Rotation    = glm::vec3(0.0f);
	glm::mat4 Perspective = glm::mat4(1.0f);
	glm::mat4 View        = glm::mat4(1.0f);

 private:
	void UpdateView() {
		glm::mat4 rotM       = glm::mat4(1.0f);
		rotM                 = glm::rotate(rotM, glm::radians(Rotation.x), glm::vec3(1, 0, 0));
		rotM                 = glm::rotate(rotM, glm::radians(Rotation.y), glm::vec3(0, 1, 0));
		rotM                 = glm::rotate(rotM, glm::radians(Rotation.z), glm::vec3(0, 0, 1));
		const glm::mat4 posM = glm::translate(glm::mat4(1.0f), Position * glm::vec3(1, 1, -1));

		if (Type == CameraType::FirstPerson) {
			View = rotM * posM;
		} else {
			View = posM * rotM;
		}
	}

	float _fovDegrees;
	float _zFar;
	float _zNear;
};
