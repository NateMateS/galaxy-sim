#include "BlackHole.h"
#include "SolarSystem.h"
#include "UI.h"
#include "Shader.h"
#include "Camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <random>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const float KM_TO_SIM_UNITS = 1.0e-8f;
const float VISUAL_SCALE_FACTOR = 3.0f;

void generateBlackHoles(std::vector<BlackHole>& blackHoles, const BlackHoleConfig& config,
	unsigned int seed, double diskRadius, double bulgeRadius) {
	blackHoles.clear();

	if (config.enableSupermassive) {
		BlackHole smbh;
		smbh.x = 0.0f;
		smbh.y = 0.0f;
		smbh.z = 0.0f;
		smbh.mass = g_currentBlackHoleMass * 1e6f;

		float rsKm = calculateSchwarzschildRadius(smbh.mass);
		smbh.eventHorizonRadius = rsKm * KM_TO_SIM_UNITS * VISUAL_SCALE_FACTOR;
		smbh.accretionDiskInnerRadius = smbh.eventHorizonRadius * 3.0f;
		smbh.accretionDiskOuterRadius = smbh.eventHorizonRadius * 20.0f;
		smbh.diskRotationAngle = 0.0f;
		smbh.diskRotationSpeed = 0.5f;

		blackHoles.push_back(smbh);
	}
}

void updateBlackHoles(std::vector<BlackHole>& blackHoles, double deltaTime) {
	for (auto& bh : blackHoles) {
		bh.diskRotationAngle += bh.diskRotationSpeed * deltaTime;
		while (bh.diskRotationAngle > 2.0f * M_PI) {
			bh.diskRotationAngle -= 2.0f * M_PI;
		}
	}
}

static unsigned int bhQuadVAO = 0;
static unsigned int bhQuadVBO = 0;

void renderBlackHoles(const std::vector<BlackHole>& blackHoles, const RenderZone& zone, const Camera& camera,
    const glm::mat4& view, const glm::mat4& projection,
    unsigned int noiseTexture, Shader* bhShader) {
    if (blackHoles.empty()) return;

    if (bhQuadVAO == 0) {
        float quadVertices[] = {
            // positions        // texCoords
            -1.0f,  0.0f, -1.0f,  0.0f, 1.0f,
            -1.0f,  0.0f,  1.0f,  0.0f, 0.0f,
             1.0f,  0.0f,  1.0f,  1.0f, 0.0f,

            -1.0f,  0.0f, -1.0f,  0.0f, 1.0f,
             1.0f,  0.0f,  1.0f,  1.0f, 0.0f,
             1.0f,  0.0f, -1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &bhQuadVAO);
        glGenBuffers(1, &bhQuadVBO);
        glBindVertexArray(bhQuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, bhQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }

    // Use Blend for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bhShader->use();

    bhShader->setMat4("view", glm::value_ptr(view));
    bhShader->setMat4("projection", glm::value_ptr(projection));
    bhShader->setFloat("time", (float)glfwGetTime());
    bhShader->setVec3("viewPos", (float)camera.posX, (float)camera.posY, (float)camera.posZ);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    bhShader->setInt("noiseTexture", 0);

	for (const auto& bh : blackHoles) {
		float visualScale = 1.5f;

        // Scale the quad to cover the disk area
        float size = bh.accretionDiskOuterRadius * visualScale;

        // Manual Matrix Construction (Column-Major) replaced with GLM
        // Translation(bh.x, bh.y, bh.z) * Scale(size)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(bh.x, bh.y, bh.z));
        model = glm::scale(model, glm::vec3(size));

        bhShader->setMat4("model", glm::value_ptr(model));
        bhShader->setVec3("centerPos", bh.x, bh.y, bh.z);

        // Shader expects normalized radii (0 to 1) relative to the quad size (which is size)
        float normInner = (bh.eventHorizonRadius * visualScale) / size; // Should be small

        bhShader->setFloat("innerRadius", normInner);
        bhShader->setFloat("outerRadius", 1.0f);

        glBindVertexArray(bhQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
