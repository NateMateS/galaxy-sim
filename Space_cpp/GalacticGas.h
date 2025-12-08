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
// Packed to 32 bytes for memory optimization
struct GasVertex {
    float orbitalRadius;       // 4 bytes
    uint32_t packedOrbital;    // 4 bytes (angle, velocity) - Half Float
    uint32_t packedOffsetsXY;  // 4 bytes (x, y) - Half Float
    uint32_t packedOffsetZSize;// 4 bytes (z, size) - Half Float
    uint32_t color;            // 4 bytes (rgba8)
    uint32_t packedTurbulence; // 4 bytes (phase, speed) - Half Float
    uint32_t _pad0;            // 4 bytes
    uint32_t _pad1;            // 4 bytes
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
