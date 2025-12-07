#include "GalacticGas.h"
#include "SolarSystem.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Static Render Resources
static unsigned int darkVAO = 0, darkVBO = 0;
static unsigned int luminousVAO = 0, luminousVBO = 0;
static size_t lastDarkCount = 0;
static size_t lastLuminousCount = 0;

struct Color4 {
    float r, g, b, a;
};

GasConfig createDefaultGasConfig() {
    GasConfig config;
    config.numMolecularClouds = 2000;
    config.numColdNeutralClouds = 8000;
    config.numWarmNeutralClouds = 12000;
    config.numWarmIonizedClouds = 200;
    config.numHotIonizedClouds = 2000;
    config.numCoronalClouds = 4000;
    config.molecularScaleHeight = 25.0f;
    config.neutralScaleHeight = 100.0f;
    config.ionizedScaleHeight = 400.0f;
    config.coronalScaleHeight = 2000.0f;
    config.enableTurbulence = true;
    config.enableDensityWaves = true;
    return config;
}

Color4 getGasColor(GasType type, float density, bool& isDarkLane) {
    Color4 color;
    isDarkLane = false;

    switch (type) {
        case GasType::MOLECULAR:
            // molecular clouds absorb light (render as dark silhouettes)
            color.r = 0.0f; color.g = 0.0f; color.b = 0.0f;
            color.a = density * 0.8f; // Stronger alpha for occlusion
            isDarkLane = true;
            break;
        case GasType::COLD_NEUTRAL:
            color.r = 0.35f; color.g = 0.28f; color.b = 0.22f;
            color.a = density * 0.03f;
            break;
        case GasType::WARM_NEUTRAL:
            color.r = 0.5f; color.g = 0.38f; color.b = 0.22f;
            color.a = density * 0.025f;
            break;
        case GasType::WARM_IONIZED:
            color.r = 0.9f; color.g = 0.25f; color.b = 0.35f;
            color.a = density * 0.03f;
            break;
        case GasType::HOT_IONIZED:
            color.r = 0.45f; color.g = 0.6f; color.b = 1.0f;
            color.a = density * 0.05f;
            break;
        case GasType::CORONAL:
            color.r = 0.55f; color.g = 0.4f; color.b = 0.7f;
            color.a = density * 0.015f;
            break;
    }
    return color;
}

// Helper to spawn a single cloud's particles
void spawnCloudParticles(std::vector<GasVertex>& targetBuffer, GasType type,
                         float orbitalRadius, float angle, float y,
                         float mass, float size, float density,
                         std::mt19937& rng, double bulgeRadius) {

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normalDist(0.0f, 1.0f);

    bool isDark;
    Color4 baseColor = getGasColor(type, density, isDark);

    // Orbital Velocity (Keplerian-ish approximation)
    float velocity = 0.5f / (sqrt(orbitalRadius / bulgeRadius) * (orbitalRadius + 1.0f));
    if (type == GasType::CORONAL) velocity *= 0.2f;

    // Generate ~15 particles per cloud to form a volume
    int numParticles = 15;
    if (type == GasType::CORONAL) numParticles = 5; // Coronal is diffuse

    for (int i = 0; i < numParticles; i++) {
        GasVertex v;

        // 1. Orbital
        v.orbitalRadius = orbitalRadius;
        v.initialAngle = angle;
        v.angularVelocity = velocity;

        // 2. Local Offset (Ellipsoid Shape)
        // Stretch along orbit (tangent)
        float stretch = 2.0f + dist(rng) * 2.0f;
        v.offsetX = normalDist(rng) * size * stretch;
        v.offsetY = y + normalDist(rng) * size * 0.5f; // Flattened Y
        v.offsetZ = normalDist(rng) * size;

        // 3. Visuals
        v.r = baseColor.r;
        v.g = baseColor.g;
        v.b = baseColor.b;

        // Vary alpha slightly for texture
        float alphaVar = 0.8f + dist(rng) * 0.4f;
        v.a = baseColor.a * alphaVar;

        // 4. Animation
        v.size = (0.5f + dist(rng)) * size * 2.0f; // Base particle size
        v.turbulencePhase = dist(rng) * 2.0f * M_PI;
        v.turbulenceSpeed = 0.5f + dist(rng) * 0.5f;

        targetBuffer.push_back(v);
    }
}

void generateGalacticGas(std::vector<GasVertex>& darkVertices, std::vector<GasVertex>& luminousVertices,
                         const GasConfig& config, unsigned int seed, double diskRadius, double bulgeRadius) {
    std::mt19937 rng(seed + 12345);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normalDist(0.0f, 1.0f);

    darkVertices.clear();
    luminousVertices.clear();

    // Reserve estimated size (approx 15 particles per cloud)
    size_t totalClouds = config.numMolecularClouds + config.numColdNeutralClouds + config.numWarmNeutralClouds +
                         config.numWarmIonizedClouds + config.numHotIonizedClouds + config.numCoronalClouds;
    luminousVertices.reserve(totalClouds * 15);
    darkVertices.reserve(config.numMolecularClouds * 15);

    const int numArms = 2;
    const double spiralTightness = 0.3;
    const double armWidth = 60.0;

    // 1. MOLECULAR CLOUDS (Dark)
    for (int i = 0; i < config.numMolecularClouds; i++) {
        // Spiral Arm Logic
        int armIndex = rng() % numArms;
        float armAngle = (armIndex * 2.0f * M_PI) / numArms;
        float radius = 100.0f + dist(rng) * (diskRadius * 0.8f);
        float spiralAngle = armAngle + spiralTightness * log(radius / 100.0f);
        float armOffset = (dist(rng) - 0.5f) * armWidth;
        float perpAngle = spiralAngle + M_PI / 2.0f;

        float x = radius * cos(spiralAngle) + armOffset * cos(perpAngle);
        float z = radius * sin(spiralAngle) + armOffset * sin(perpAngle);
        float orbRadius = sqrt(x*x + z*z);
        float angle = atan2(z, x);
        float y = normalDist(rng) * config.molecularScaleHeight;

        float size = 10.0f + dist(rng) * 20.0f;
        float density = 0.7f + dist(rng) * 0.3f;

        spawnCloudParticles(darkVertices, GasType::MOLECULAR, orbRadius, angle, y, 1.0f, size, density, rng, bulgeRadius);
    }

    // 2. COLD NEUTRAL (Luminous)
    for (int i = 0; i < config.numColdNeutralClouds; i++) {
        float diskScale = diskRadius * 0.3f;
        float u = dist(rng);
        float radius = -diskScale * log(1.0f - u * 0.95f + 1e-8f);
        if (radius > diskRadius * 1.2f) radius = diskRadius * 1.2f;

        float theta = dist(rng) * 2.0f * M_PI;
        float y = normalDist(rng) * config.neutralScaleHeight;
        float size = 8.0f + dist(rng) * 15.0f;

        spawnCloudParticles(luminousVertices, GasType::COLD_NEUTRAL, radius, theta, y, 1.0f, size, 0.5f, rng, bulgeRadius);
    }

    // 3. WARM NEUTRAL
    for (int i = 0; i < config.numWarmNeutralClouds; i++) {
        float diskScale = diskRadius * 0.35f;
        float u = dist(rng);
        float radius = -diskScale * log(1.0f - u * 0.95f + 1e-8f);
        if (radius > diskRadius * 1.5f) radius = diskRadius * 1.5f;

        float theta = dist(rng) * 2.0f * M_PI;
        float y = normalDist(rng) * config.neutralScaleHeight * 1.5f;
        float size = 15.0f + dist(rng) * 25.0f;

        spawnCloudParticles(luminousVertices, GasType::WARM_NEUTRAL, radius, theta, y, 1.0f, size, 0.4f, rng, bulgeRadius);
    }

    // 4. WARM IONIZED (Spiral Arms)
    for (int i = 0; i < config.numWarmIonizedClouds; i++) {
        // Spiral Arm Logic
        int armIndex = rng() % numArms;
        float armAngle = (armIndex * 2.0f * M_PI) / numArms;
        float radius = 100.0f + dist(rng) * (diskRadius * 0.8f);
        float spiralAngle = armAngle + spiralTightness * log(radius / 100.0f);
        float armOffset = (dist(rng) - 0.5f) * armWidth * 0.8f; // Tighter
        float perpAngle = spiralAngle + M_PI / 2.0f;

        float x = radius * cos(spiralAngle) + armOffset * cos(perpAngle);
        float z = radius * sin(spiralAngle) + armOffset * sin(perpAngle);
        float orbRadius = sqrt(x*x + z*z);
        float angle = atan2(z, x);
        float y = normalDist(rng) * config.molecularScaleHeight * 2.0f;
        float size = 8.0f + dist(rng) * 15.0f;

        spawnCloudParticles(luminousVertices, GasType::WARM_IONIZED, orbRadius, angle, y, 1.0f, size, 0.8f, rng, bulgeRadius);
    }

    // 5. HOT IONIZED
    for (int i = 0; i < config.numHotIonizedClouds; i++) {
        float diskScale = diskRadius * 0.4f;
        float u = dist(rng);
        float radius = -diskScale * log(1.0f - u * 0.9f + 1e-8f);
        if (radius > diskRadius * 1.3f) radius = diskRadius * 1.3f;

        float theta = dist(rng) * 2.0f * M_PI;
        float y = normalDist(rng) * config.ionizedScaleHeight;
        float size = 20.0f + dist(rng) * 30.0f;

        spawnCloudParticles(luminousVertices, GasType::HOT_IONIZED, radius, theta, y, 1.0f, size, 0.3f, rng, bulgeRadius);
    }

    // 6. CORONAL
    for (int i = 0; i < config.numCoronalClouds; i++) {
        float theta = dist(rng) * 2.0f * M_PI;
        float phi = acos(2.0f * dist(rng) - 1.0f);
        float radius = pow(dist(rng), 0.5f) * diskRadius * 2.5f;

        float x = radius * sin(phi) * cos(theta);
        float z = radius * cos(phi); // Simplified mapping
        float orbRadius = sqrt(x*x + z*z); // Roughly
        float y = radius * sin(phi) * sin(theta);

        spawnCloudParticles(luminousVertices, GasType::CORONAL, orbRadius, theta, y, 1.0f, 100.0f, 0.1f, rng, bulgeRadius);
    }
}

void uploadGasData(unsigned int& vao, unsigned int& vbo, const std::vector<GasVertex>& vertices) {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GasVertex), vertices.data(), GL_STATIC_DRAW);

    // 13 Floats Total Stride
    GLsizei stride = sizeof(GasVertex);

    // 1. Orbital (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GasVertex, orbitalRadius));

    // 2. Offset (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GasVertex, offsetX));

    // 3. Color (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GasVertex, r));

    // 4. Params (vec3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GasVertex, size));

    glBindVertexArray(0);
}

void renderGalacticGas(const std::vector<GasVertex>& darkVertices, const std::vector<GasVertex>& luminousVertices,
                       float time, unsigned int depthTexture, float screenWidth, float screenHeight,
                       const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection, Shader* gasShader) {

    // Check if upload is needed
    bool darkDirty = (darkVertices.size() != lastDarkCount);
    bool lumDirty = (luminousVertices.size() != lastLuminousCount);

    if (darkDirty) {
        uploadGasData(darkVAO, darkVBO, darkVertices);
        lastDarkCount = darkVertices.size();
    }

    if (lumDirty) {
        uploadGasData(luminousVAO, luminousVBO, luminousVertices);
        lastLuminousCount = luminousVertices.size();
    }

    gasShader->use();
    gasShader->setMat4("view", glm::value_ptr(view));
    gasShader->setMat4("projection", glm::value_ptr(projection));
    gasShader->setFloat("u_Time", time);
    gasShader->setFloat("pointScale", 400.0f); // Tunable

    // Soft Particles Uniforms
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depthTexture);
    gasShader->setInt("depthMap", 1);

    gasShader->setFloat("zNear", 0.1f);
    gasShader->setFloat("zFar", 20000.0f);
    gasShader->setFloat("softnessScale", 0.05f); // 1.0 / 20.0 units

    glEnable(GL_BLEND);
    // Enable Depth Testing so particles are correctly occluded by opaque objects (planets/stars)
    glEnable(GL_DEPTH_TEST);
    // Disable Depth Writing so transparent particles don't occlude each other or the background
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // 1. Dark Lanes (Occlusion/Absorption)
    // Use standard alpha blending with black color (0,0,0) to darken the background.
    // Result = (0,0,0) * SrcAlpha + Background * (1 - SrcAlpha) = Background * (1 - SrcAlpha)
    if (darkVertices.size() > 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(darkVAO);
        glDrawArrays(GL_POINTS, 0, (GLsizei)darkVertices.size());
    }

    // 2. Luminous (Additive/Emission)
    // Use additive blending for glowing gas layers.
    // Result = Source * SrcAlpha + Background * 1
    if (luminousVertices.size() > 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindVertexArray(luminousVAO);
        glDrawArrays(GL_POINTS, 0, (GLsizei)luminousVertices.size());
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}
