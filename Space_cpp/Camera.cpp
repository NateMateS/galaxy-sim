#include "Camera.h"
#include "SolarSystem.h"
#include "UI.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

void getCameraMatrices(const Camera& camera, int width, int height, const SolarSystem& solarSystem, glm::mat4& view, glm::mat4& projection) {
    // Projection Matrix
    float fov = glm::radians(45.0f);
    float aspect = (float)width / (float)height;
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;

    projection = glm::perspective(fov, aspect, nearPlane, farPlane);

    // View Matrix (ModelView in legacy terms, but usually we separate Model and View)
    // 'view' here represents the Camera's transform (World -> Camera).

    // Quaternion Rotation (Inverse/Conjugate for View Matrix)
    view = glm::mat4_cast(glm::conjugate(camera.orientation));

    view = glm::translate(view, glm::vec3(-camera.posX, -camera.posY, -camera.posZ));

    if (camera.freeZoomMode) {
        // In GLM, operations are applied in reverse order of multiplication if written as M = T * R * S
        view = glm::translate(view, glm::vec3(solarSystem.centerX, solarSystem.centerY, solarSystem.centerZ));
        view = glm::scale(view, glm::vec3(camera.zoom));
        view = glm::translate(view, glm::vec3(-solarSystem.centerX, -solarSystem.centerY, -solarSystem.centerZ));
    } else {
        view = glm::scale(view, glm::vec3(camera.zoom));
    }
}

void processInput(GLFWwindow* window, Camera& camera, const UIState* uiState) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (uiState && uiState->isVisible) return;

	double speedMultiplier = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 2.0 : 1.0;
	double currentSpeed = camera.moveSpeed * speedMultiplier;

    // Calculate direction vectors from quaternion
    glm::vec3 forward = camera.orientation * glm::vec3(0.0, 0.0, -1.0);
    glm::vec3 right = camera.orientation * glm::vec3(1.0, 0.0, 0.0);
    glm::vec3 up = camera.orientation * glm::vec3(0.0, 1.0, 0.0);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		camera.posX += forward.x * currentSpeed;
		camera.posY += forward.y * currentSpeed;
		camera.posZ += forward.z * currentSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		camera.posX -= forward.x * currentSpeed;
		camera.posY -= forward.y * currentSpeed;
		camera.posZ -= forward.z * currentSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		camera.posX -= right.x * currentSpeed;
		camera.posY -= right.y * currentSpeed;
		camera.posZ -= right.z * currentSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		camera.posX += right.x * currentSpeed;
		camera.posY += right.y * currentSpeed;
		camera.posZ += right.z * currentSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera.posY += currentSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		camera.posY -= currentSpeed;
	}

    // Optional: Q/E for Roll
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
         glm::quat roll = glm::angleAxis((float)(-camera.lookSpeed * 2.0), glm::vec3(0, 0, 1));
         camera.orientation = glm::normalize(camera.orientation * roll);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
         glm::quat roll = glm::angleAxis((float)(camera.lookSpeed * 2.0), glm::vec3(0, 0, 1));
         camera.orientation = glm::normalize(camera.orientation * roll);
    }
}
