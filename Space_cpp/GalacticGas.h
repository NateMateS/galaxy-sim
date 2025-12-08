#pragma once
#include <vector>
#include <glm/glm.hpp>

struct RenderZone;

enum class GasType {
    MOLECULAR,
    COLD_NEUTRAL,
    WARM_NEUTRAL,
    WARM_IONIZED,
    HOT_IONIZED,
    CORONAL
};

// Flattened stateless particle structure for GPU simulation
// Aligned to 64 bytes (4x vec4) for optimal GPU access
struct GasVertex {
    // 1. Orbital Mechanics (vec4: radius, angle, velocity, padding)
    float orbitalRadius;
    float initialAngle; // Where the cloud started
    float angularVelocity; // How fast it orbits the galaxy
    float _pad0;

    // 2. Local Shape (vec4: x, y, z offsets from cloud center, padding)
    float offsetX, offsetY, offsetZ;
    float _pad1;

    // 3. Visuals (vec4: r, g, b, a)
    float r, g, b, a;

    // 4. Animation Params (vec4: size, turbPhase, turbSpeed, padding)
    float size;
    float turbulencePhase;
    float turbulenceSpeed;
    float _pad2;
};

struct GasConfig {
    int numMolecularClouds;
    int numColdNeutralClouds;
    int numWarmNeutralClouds;
    int numWarmIonizedClouds;
    int numHotIonizedClouds;
    int numCoronalClouds;

    // distribution
    float molecularScaleHeight;
    float neutralScaleHeight;
    float ionizedScaleHeight;
    float coronalScaleHeight;

    bool enableTurbulence;
    bool enableDensityWaves;
};

GasConfig createDefaultGasConfig();

// Note: This now generates static vertices instead of dynamic objects
void generateGalacticGas(std::vector<GasVertex>& darkVertices, std::vector<GasVertex>& luminousVertices, const GasConfig& config, unsigned int seed, double diskRadius, double bulgeRadius);

// Prepare resources and run compute shader for culling
void prepareGalacticGas(const std::vector<GasVertex>& darkVertices, const std::vector<GasVertex>& luminousVertices, float time, unsigned int depthTexture, float screenWidth, float screenHeight, const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection);

// Draw the particles (split into passes)
void drawDarkGas(class Shader* gasShader, const glm::mat4& view, const glm::mat4& projection, float time, unsigned int depthTexture);
void drawLuminousGas(class Shader* gasShader, const glm::mat4& view, const glm::mat4& projection, float time, unsigned int depthTexture, bool quarterRes);

const float MOLECULAR_TEMP = 20.0f;          // 10-50 K
const float COLD_NEUTRAL_TEMP = 80.0f;       // 50-100 K
const float WARM_NEUTRAL_TEMP = 8000.0f;     // 6000-10000 K
const float WARM_IONIZED_TEMP = 8000.0f;     // ~8000 K
const float HOT_IONIZED_TEMP = 1e6f;         // ~1 million K
const float CORONAL_TEMP = 5e6f;             // 1-10 million K
