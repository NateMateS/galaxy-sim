#pragma once
#include <vector>
#include <glm/glm.hpp>

struct RenderZone;

// Optimized Star Input for Compute Shader (Aligned to vec4)
struct StarInput {
    // x: radius, y: initialAngle, z: y_pos, w: angularVelocity
    float radius;
    float angle;
    float y;
    float velocity;

    // x: r, y: g, z: b, w: brightness
    float r;
    float g;
    float b;
    float brightness;
};

// Layout: radius, angle, y, velocity, r, g, b, brightness
// Size: 8 floats = 32 bytes
struct GalaxyConfig {
	int numStars;
	int numSpiralArms;
	double spiralTightness;
	double armWidth;
	double diskRadius;
	double bulgeRadius;
	double diskHeight;
	double bulgeHeight;
	double armDensityBoost;
	unsigned int seed;

	double rotationSpeed;	// Base rotation multiplier
};

void initStars();
void cleanupStars();
void generateStarField(std::vector<StarInput>& stars, const GalaxyConfig& config);
void uploadStarData(const std::vector<StarInput>& stars);
void renderStars(const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, float time);
