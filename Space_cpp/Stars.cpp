#include "Stars.h"
#include "SolarSystem.h"
#include "Shader.h"
#include "Window.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <random>
#include <memory>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Star type colors (based on stellar classification: O, B, A, F, G, K, M)
namespace {
    struct StarType {
        float r, g, b;
        float probability;
    };

    const StarType starTypes[] = {
        {0.6f, 0.7f, 1.0f, 0.05f},   // O - Blue (very hot, rare)
        {0.7f, 0.8f, 1.0f, 0.10f},   // B - Blue-white (hot)
        {0.9f, 0.9f, 1.0f, 0.15f},   // A - White (hot)
        {1.0f, 1.0f, 0.9f, 0.20f},   // F - Yellow-white
        {1.0f, 1.0f, 0.7f, 0.25f},   // G - Yellow (like our Sun)
        {1.0f, 0.8f, 0.6f, 0.15f},   // K - Orange
        {1.0f, 0.6f, 0.5f, 0.10f}    // M - Red (cool, common)
    };
}

// Modern GL State
static std::unique_ptr<Shader> starShader;
static unsigned int starVAO = 0;
static unsigned int starVBO = 0;

void initStars() {
    if (starVAO != 0) {
        cleanupStars();
    }

    // Exception will propagate to main for proper handling
    starShader = std::make_unique<Shader>("assets/shaders/star.vert", "assets/shaders/star.frag");

    glEnable(GL_PROGRAM_POINT_SIZE);

    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);

    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);

    // Layout: x, y, z, r, g, b, brightness, radius, angle, angularVelocity
    // Size: 10 floats = 40 bytes
    GLsizei stride = sizeof(Star);

    // 0: aPos (x,y,z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // 1: aColor (r,g,b)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 2: aBrightness
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // 3: aRadius
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // 4: aAngle
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(4);

    // 5: aVelocity
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(5);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void cleanupStars() {
    if (starVAO) {
        glDeleteVertexArrays(1, &starVAO);
        starVAO = 0;
    }
    if (starVBO) {
        glDeleteBuffers(1, &starVBO);
        starVBO = 0;
    }
    starShader.reset();
}

void uploadStarData(const std::vector<Star>& stars) {
    if (starVAO == 0) return;

    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);

    // Dynamic Draw since we might regenerate the galaxy
    glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(Star), stars.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
}

void generateStarField(std::vector<Star>& stars, const GalaxyConfig& config) {
	std::mt19937 rng(config.seed);
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	std::normal_distribution<float> normalDist(0.0f, 1.0f);

	stars.clear();
	stars.reserve(config.numStars);

	for (int i = 0; i < config.numStars; i++) {
		Star star;

		// Decide if star is in bulge or disk
		// bulge = the spherical central region
		// disk = the flat rotating part with spiral arms
		bool inBulge = dist(rng) < 0.15f; // 15%

		if (inBulge) {
			// spherical distribution
			float theta = dist(rng) * 2.0f * M_PI; // horizontal rotation around the Z axis
			float phi = acos(2.0f * dist(rng) - 1.0f); // vertical angle from the top of sphere
			float radius = pow(dist(rng), 1.0f / 3.0f) * config.bulgeRadius;

			star.x = radius * sin(phi) * cos(theta);
			star.y = radius * sin(phi) * sin(theta);
			star.z = radius * cos(phi);

			// rotation (in union) for bulge stars
			star.radius = sqrt(star.x * star.x + star.z * star.z);
			star.angle = atan2(star.z, star.x);

			// higher velocity since bulge rotates faster
			star.angularVelocity = config.rotationSpeed * 0.5f / (config.bulgeRadius + 1.0f);
		}
		else {
			// disk & arms

			// Exponential disk sampling
			// Radial surface density: Sigma(r) ∝ exp(-r/rd)
			// Radial PDF (per radius) ∝ r * exp(-r/rd)
			// CDF: F(r) = 1 - (1 + r/rd) * exp(-r/rd)
			// Invert F numerically (Newton) to get r for uniform u in [0,1].
			auto sampleExponentialDiskRadius = [&](float diskScale) -> float {
				float u = dist(rng);
				// initial guess: exponential inverse (not exact for r*e^{-r/rd} but a reasonable start)
				float r = -diskScale * std::log(1.0f - u + 1e-8f);
				// Newton iteration to solve F(r) - u = 0
				for (int it = 0; it < 10; ++it) {
					float t = r / diskScale;
					float expNegT = std::exp(-t);
					float F = 1.0f - (1.0f + t) * expNegT;        // F(r)
					float G = F - u;							  // want G == 0

					if (std::fabs(G) < 1e-6f) break;
					// dF/dr = (r / rd^2) * exp(-r/rd)
					float dFdr = (r == 0.0f) ? (1.0f / (diskScale)) * 0.0f : (r / (diskScale * diskScale)) * expNegT;
					// fallback if derivative is too small
					if (dFdr <= 1e-12f) break;
					float delta = G / dFdr;
					r -= delta;
					if (r < 0.0f) { r = 0.0f; break; }
				}
				return r;
				};

			float diskScale = static_cast<float>(config.diskRadius) * 0.25f; // tune to taste
			float radius = sampleExponentialDiskRadius(diskScale);

			float maxRadius = static_cast<float>(config.diskRadius) * 2.0f; // allow 2x radius to allow stars beyond diskRadius to fade out (not creating an uniform circle)
			if (radius > maxRadius) {
				radius = maxRadius;
			}

			// Base angle
			float theta = dist(rng) * 2.0f * M_PI;

			// Calculate distance to nearest spiral arm
			float minArmDistance = 1e10f;

			for (int arm = 0; arm < config.numSpiralArms; arm++) {
				// Logarithmic spiral: r = a * e^(b * theta)
				// Solving for theta: theta = ln(r/a) / b
				float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;

				// Calculate where this radius intersects the spiral arm
				float spiralTheta = log(radius / config.bulgeRadius) / config.spiralTightness + armOffset;

				// Normalize angle difference to [-PI, PI]
				float angleDiff = theta - spiralTheta;
				while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
				while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;

				// Convert angle difference to distance at this radius
				float armDistance = fabs(angleDiff * radius);
				minArmDistance = fmin(minArmDistance, armDistance);
			}

			float radiusNorm = radius / static_cast<float>(config.diskRadius);
			float edgeFactor = (radiusNorm > 1.0f) ? 1.0f : radiusNorm; // Clamp for calculation
			float effectiveArmWidth = config.armWidth * (1.0f + edgeFactor * 1.5f); // Arms get wider towards edges

			// Stars close to arms have high probability, far from arms very low
			float armProximity = exp(-minArmDistance * minArmDistance / (effectiveArmWidth * effectiveArmWidth));

			float acceptProbability;
			if (radius > config.diskRadius) {
				// we over the disk radius, this is the outlier region
				// split into multiple zones for smoother transition
				float excessRadius = radius - config.diskRadius;
				float fadeScale = config.diskRadius * 0.15f;

				float outlierFactor = exp(-excessRadius / fadeScale);

				// quadratic suppression for extreme outliers
				if (radiusNorm > 1.3f) {
					float extremeFactor = 1.3f / radiusNorm;
					outlierFactor *= extremeFactor * extremeFactor;
				}

				// 8% of normal density
				acceptProbability = outlierFactor * 0.08f;
			}
			else if (radius > config.diskRadius * 0.85f) {
				// Transition zone (85% - 100% of diskRadius) with gradual fadeout
				float transitionFactor = (config.diskRadius - radius) / (config.diskRadius * 0.15f);
				transitionFactor = 0.5f + 0.5f * transitionFactor;

				float densityWeight = armProximity * config.armDensityBoost;
				acceptProbability = (1.0f + densityWeight) / (1.0f + config.armDensityBoost);

				// 80% rejection for inter-arm regions
				if (armProximity < 0.3f) {
					acceptProbability *= 0.2f;
				}

				acceptProbability *= transitionFactor;
			}
			else {
				float densityWeight = armProximity * config.armDensityBoost;
				acceptProbability = (1.0f + densityWeight) / (1.0f + config.armDensityBoost);

				// 80% rejection for inter-arm regions
				if (armProximity < 0.3f) {
					acceptProbability *= 0.2f;
				}
			}

			if (dist(rng) > acceptProbability) {
				i--;  // Retry this star
				continue;
			}

			// positional noise for irregular edges
			float noiseScale = 15.0f * (1.0f + radiusNorm * 0.8f);
			float noise = normalDist(rng) * noiseScale;

			// radial scatter at edges
			float radialScatter = normalDist(rng) * 20.0f * radiusNorm * radiusNorm;

			float effectiveRadius = radius + noise * 0.3f + radialScatter;

			star.x = effectiveRadius * cos(theta);
			star.z = effectiveRadius * sin(theta);

			// Y position (disk height with Gaussian distribution)
			float heightScale = config.diskHeight * (1.0f - edgeFactor * 0.5f);
			star.y = normalDist(rng) * heightScale;

			// rotation for disk stars
			star.radius = effectiveRadius;
			star.angle = theta;
			// outer stars rotate slower
			star.angularVelocity = config.rotationSpeed * 1.0f / (sqrt(radius / config.bulgeRadius) * (radius + 1.0f));
		}

		// select star type
		float typeRoll = dist(rng);
		float cumulative = 0.0f;
		int selectedType = 6; // default M type

		for (int t = 0; t < 7; t++) {
			cumulative += starTypes[t].probability;
			if (typeRoll <= cumulative) {
				selectedType = t;
				break;
			}
		}

		star.r = starTypes[selectedType].r;
		star.g = starTypes[selectedType].g;
		star.b = starTypes[selectedType].b;

		// stars in bulge tend to be older
		float distFromCenter = sqrt(star.x * star.x + star.y * star.y + star.z * star.z);
		if (distFromCenter < config.bulgeRadius) {
			star.brightness = 0.4f + dist(rng) * 0.4f; // dim
		}
		else {
			star.brightness = 0.3f + dist(rng) * 0.7f; // bright

			// stars in spiral arms are brighter
			float minArmDist = 1e10f;

			for (int arm = 0; arm < config.numSpiralArms; arm++) {
				float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;
				float spiralTheta = log(star.radius / config.bulgeRadius) / config.spiralTightness + armOffset;
				float angleDiff = star.angle - spiralTheta;

				while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
				while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;

				float armDist = fabs(angleDiff * star.radius);

				minArmDist = fmin(minArmDist, armDist);
			}
			float armBrightness = exp(-minArmDist * minArmDist / (config.armWidth * config.armWidth * 4.0f));

			star.brightness += armBrightness * 0.3f;
			if (star.brightness > 1.0f) star.brightness = 1.0f;
		}

		stars.push_back(star);
	}
}


void renderStars(const std::vector<Star>& stars, const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection) {
    if (!starShader) return;

    // Note: GL_POINT_SPRITE is deprecated in Core Profile (3.2+) and always enabled.
    // Texture coord replacement is handled by gl_PointCoord automatically.
    // Ensure program point size is enabled (usually is, but good for safety)
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Enable additive blending for glowing effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // Disable depth writing so stars blend instead of occluding each other
    glDepthMask(GL_FALSE);

    starShader->use();

    starShader->setMat4("view", glm::value_ptr(view));
    starShader->setMat4("projection", glm::value_ptr(projection));
    starShader->setFloat("time", (float)glfwGetTime());
    starShader->setFloat("screenHeight", (float)HEIGHT);

    glBindVertexArray(starVAO);
    // Note: Data is now static and uploaded via uploadStarData()

    glDrawArrays(GL_POINTS, 0, (GLsizei)stars.size());

    glBindVertexArray(0);
    glUseProgram(0);

    // Cleanup state
    glDepthMask(GL_TRUE); // Re-enable depth writing
    glDisable(GL_BLEND);
}
