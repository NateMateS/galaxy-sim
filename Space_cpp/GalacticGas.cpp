#include "GalacticGas.h"
#include "SolarSystem.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// GPU Resources Container
struct GasResources {
    unsigned int inputSSBO = 0;
    unsigned int outputSSBO = 0;
    unsigned int indirectBuffer = 0;
    unsigned int vao = 0; // Empty VAO, VBO is the Output SSBO
    size_t capacity = 0;
    size_t count = 0;
};

static GasResources darkGasRes;
static GasResources lumGasRes;

static unsigned int computeProgram = 0;
static size_t lastDarkCount = 0;
static size_t lastLuminousCount = 0;

struct DrawCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int first;
    unsigned int baseInstance;
};

struct Color4 {
    float r, g, b, a;
};

// --- Shader Management ---
static void checkCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

static void initCompute() {
    if (computeProgram != 0) return;

    std::string code;
    std::ifstream file("assets/shaders/gas_cull.comp");
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open assets/shaders/gas_cull.comp" << std::endl;
        return;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    code = ss.str();
    const char* cCode = code.c_str();

    unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &cCode, NULL);
    glCompileShader(shader);
    checkCompileErrors(shader, "COMPUTE");

    computeProgram = glCreateProgram();
    glAttachShader(computeProgram, shader);
    glLinkProgram(computeProgram);
    checkCompileErrors(computeProgram, "PROGRAM");
    glDeleteShader(shader);
}

static void initGasResources(GasResources& res) {
    if (res.vao != 0) return;

    glGenBuffers(1, &res.inputSSBO);
    glGenBuffers(1, &res.outputSSBO);
    glGenBuffers(1, &res.indirectBuffer);

    glGenVertexArrays(1, &res.vao);
    glBindVertexArray(res.vao);
    glBindBuffer(GL_ARRAY_BUFFER, res.outputSSBO);

    // Layout matches GasRender struct in shader:
    // vec4 pos_depth (16), vec4 color (16), float size (4), padding (12) = 48 bytes?
    // Wait, GasRender in shader:
    // vec4 pos_depth; // 0
    // vec4 color;     // 16
    // float size;     // 32
    // float _pad[3];  // 36 -> 48 bytes total?
    // Shader layout std430 aligns structs.
    // vec4 is 16-aligned.
    // 0: vec4 (16)
    // 16: vec4 (16)
    // 32: float (4)
    // 36: float (4)
    // 40: float (4)
    // 44: float (4)
    // Total 48 bytes.
    // Stride = 48.

    // Attrib 0: Pos (vec3) - from pos_depth.xyz
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 48, (void*)0);

    // Attrib 1: Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 48, (void*)16);

    // Attrib 2: Linear Depth (float) - from pos_depth.w
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 48, (void*)12); // Offset 12 is .w of first vec4

    // Attrib 3: Size (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 48, (void*)32);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Init Indirect Buffer
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, res.indirectBuffer);
    DrawCommand cmd = {0, 1, 0, 0};
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand), &cmd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

static void uploadGasData(GasResources& res, const std::vector<GasVertex>& vertices) {
    if (vertices.empty()) return;
    res.count = vertices.size();

    // 1. Upload Input (Static)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.inputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(GasVertex), vertices.data(), GL_STATIC_DRAW);

    // 2. Allocate Output (Dynamic)
    // 48 bytes per vertex
    size_t outputSize = vertices.size() * 48;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.outputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, outputSize, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// --- Configuration ---

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
        v._pad0 = 0;

        // 2. Local Offset (Ellipsoid Shape)
        // Stretch along orbit (tangent)
        float stretch = 2.0f + dist(rng) * 2.0f;
        v.offsetX = normalDist(rng) * size * stretch;
        v.offsetY = y + normalDist(rng) * size * 0.5f; // Flattened Y
        v.offsetZ = normalDist(rng) * size;
        v._pad1 = 0;

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
        v._pad2 = 0;

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
        float z = radius * cos(phi);
        float orbRadius = sqrt(x*x + z*z); // Roughly
        float y = radius * sin(phi) * sin(theta);
        spawnCloudParticles(luminousVertices, GasType::CORONAL, orbRadius, theta, y, 1.0f, 100.0f, 0.1f, rng, bulgeRadius);
    }
}

void prepareGalacticGas(const std::vector<GasVertex>& darkVertices, const std::vector<GasVertex>& luminousVertices,
                        float time, unsigned int depthTexture, float screenWidth, float screenHeight,
                        const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection) {
    // Init resources if needed
    initCompute();
    initGasResources(darkGasRes);
    initGasResources(lumGasRes);

    // Check for updates
    if (darkVertices.size() != lastDarkCount) {
        uploadGasData(darkGasRes, darkVertices);
        lastDarkCount = darkVertices.size();
    }
    if (luminousVertices.size() != lastLuminousCount) {
        uploadGasData(lumGasRes, luminousVertices);
        lastLuminousCount = luminousVertices.size();
    }

    // --- Compute Pass ---
    glUseProgram(computeProgram);
    glUniform1f(glGetUniformLocation(computeProgram, "pointScale"), 200.0f);

    // Bind Depth Map for Occlusion Culling
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(glGetUniformLocation(computeProgram, "depthMap"), 0);

    // Uniforms for Coordinate Conversion & Stochastic LOD
    glUniform1f(glGetUniformLocation(computeProgram, "screenWidth"), screenWidth);
    glUniform1f(glGetUniformLocation(computeProgram, "screenHeight"), screenHeight);
    glUniform1f(glGetUniformLocation(computeProgram, "zNear"), 0.1f);
    glUniform1f(glGetUniformLocation(computeProgram, "zFar"), 20000.0f);

    auto dispatchBatch = [](GasResources& res) {
        if (res.count == 0) return;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, res.inputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, res.outputSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, res.indirectBuffer);

        DrawCommand resetCmd = {0, 1, 0, 0};
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, res.indirectBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawCommand), &resetCmd);

        glDispatchCompute((unsigned int)((res.count + 255) / 256), 1, 1);
    };

    dispatchBatch(darkGasRes);
    dispatchBatch(lumGasRes);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void drawDarkGas(Shader* gasShader, const glm::mat4& view, const glm::mat4& projection, float time, unsigned int depthTexture) {
    if (darkGasRes.count == 0) return;

    // --- Render Pass ---
    gasShader->use();
    gasShader->setFloat("u_Time", time);
    gasShader->setFloat("resolutionScale", 1.0f); // Always full res for dark gas
    gasShader->setFloat("pointMultiplier", 1.0f); // Full resolution size

    // Bind Depth Map for Soft Particles
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
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

    // Render Dark Lanes (Occlusion/Absorption)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(darkGasRes.vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, darkGasRes.indirectBuffer);
    glDrawArraysIndirect(GL_POINTS, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void drawLuminousGas(Shader* gasShader, const glm::mat4& view, const glm::mat4& projection, float time, unsigned int depthTexture, bool quarterRes) {
    if (lumGasRes.count == 0) return;

    gasShader->use();
    gasShader->setFloat("u_Time", time);

    // If quarterRes is true, we need to scale gl_FragCoord in the shader to sample the full-res depth map correctly
    // Low-Res path uses Quarter-Res (4.0 scale)
    if (!quarterRes) {
        gasShader->setFloat("resolutionScale", 1.0f);
    }
    // Scale points down by 0.25 if rendering to quarter-res buffer to preserve screen coverage ratio and reduce fill rate
    gasShader->setFloat("pointMultiplier", quarterRes ? 0.25f : 1.0f);

    // Bind Depth Map for Soft Particles
    if (!quarterRes) {
        // Full Res: Use Hardware Depth Test + Manual Softness Read
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        gasShader->setInt("depthMap", 1);

        glEnable(GL_DEPTH_TEST);
    } else {
        // Quarter Res: Disable Hardware Depth Test
        // We are rendering to an offscreen buffer with NO depth attachment.
        // Depth testing is done manually in the pixel shader against the downsampled depth texture.
        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        gasShader->setInt("quarterResLinearDepth", 0);
    }

    if (!quarterRes) {
        gasShader->setFloat("zNear", 0.1f);
        gasShader->setFloat("zFar", 20000.0f);
    }
    gasShader->setFloat("softnessScale", 0.05f);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Render Luminous (Additive/Emission)
    // For Quarter-Res R11G11B10F, we accumulate light: SrcRGB * SrcAlpha + DstRGB
    // This works perfectly as the buffer has no alpha to mess up.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindVertexArray(lumGasRes.vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, lumGasRes.indirectBuffer);
    glDrawArraysIndirect(GL_POINTS, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
