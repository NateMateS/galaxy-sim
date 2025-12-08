#pragma once
#include <vector>
#include <glm/glm.hpp>

struct RenderZone;

// Packed Star Input (16 bytes)
struct StarInput {
    float radius;            // 4 bytes
    uint32_t packedOrbital;  // 4 bytes (angle, velocity) - Half2x16
    uint32_t packedYBright;  // 4 bytes (y, brightness) - Half2x16
    uint32_t color;          // 4 bytes (rgba8)
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
