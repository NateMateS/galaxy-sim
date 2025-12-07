#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Camera {
    double posX = 0.0, posY = 0.0, posZ = 10.0;
    glm::quat orientation = glm::quat(1.0, 0.0, 0.0, 0.0); // w, x, y, z
    double zoom = 1.0;
    double zoomLevel = 0.001;
    double moveSpeed = 0.1;
    double lookSpeed = 0.002;
    bool freeZoomMode = false;
};

struct SolarSystem;
struct UIState;

// Replaces legacy setupCamera
void getCameraMatrices(const Camera& camera, int width, int height, const SolarSystem& solarSystem, glm::mat4& view, glm::mat4& projection);

void processInput(struct GLFWwindow* window, Camera& camera, const UIState* uiState = nullptr);
