#include "Stars.h"
#include "SolarSystem.h"
#include "Shader.h"
#include "Window.h"
#include "TextureGenerator.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <random>
#include <memory>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/packing.hpp>
#include <fstream>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// GPU Resources
static unsigned int inputSSBO = 0;
static unsigned int outputSSBO = 0;
static unsigned int indirectBuffer = 0;
static unsigned int starVAO = 0; // Empty VAO for drawing
static unsigned int computeProgram = 0;
static std::unique_ptr<Shader> starRenderShader;
static unsigned int starSpriteTexture = 0;

// Capacity
static size_t maxStars = 0;

struct DrawCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int first;
    unsigned int baseInstance;
};

// Star type colors
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

static void checkCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

void initStars() {
    if (computeProgram != 0) cleanupStars();

    // 1. Compile Compute Shader
    std::string code;
    std::ifstream file("assets/shaders/star_cull.comp");
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open assets/shaders/star_cull.comp" << std::endl;
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

    // 2. Initialize Render Shader
    starRenderShader = std::make_unique<Shader>("assets/shaders/star.vert", "assets/shaders/star.frag");
    starRenderShader->use();
    starRenderShader->setInt("spriteTexture", 0);

    // Generate Sprite Texture
    if (starSpriteTexture == 0) {
        starSpriteTexture = TextureGenerator::GenerateGlowSprite(128, 128);
    }

    // 3. Create Buffers
    glGenBuffers(1, &inputSSBO);
    glGenBuffers(1, &outputSSBO);
    glGenBuffers(1, &indirectBuffer);

    // Create empty VAO
    glGenVertexArrays(1, &starVAO);
    glBindVertexArray(starVAO);

    // Bind Output Buffer as VBO for the VAO
    glBindBuffer(GL_ARRAY_BUFFER, outputSSBO);

    // Packed to 20 bytes (Pos(12) + Color(4) + Size(4))
    const int STRIDE = 20;

    // Attrib 0: Pos (vec3) - px, py, pz
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)0);

    // Attrib 1: Color (vec4 unpacked from uint)
    // Offset 12 (after 3 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, STRIDE, (void*)12);

    // Attrib 2: Size (float)
    // Offset 16 (after color)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, STRIDE, (void*)16);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Setup Indirect Buffer
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    DrawCommand cmd = {0, 1, 0, 0};
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand), &cmd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void cleanupStars() {
    if (computeProgram) glDeleteProgram(computeProgram);
    if (inputSSBO) glDeleteBuffers(1, &inputSSBO);
    if (outputSSBO) glDeleteBuffers(1, &outputSSBO);
    if (indirectBuffer) glDeleteBuffers(1, &indirectBuffer);
    if (starVAO) glDeleteVertexArrays(1, &starVAO);
    if (starSpriteTexture) glDeleteTextures(1, &starSpriteTexture);
    starRenderShader.reset();
}

void uploadStarData(const std::vector<StarInput>& stars) {
    if (stars.empty()) return;
    maxStars = stars.size();

    // 1. Upload Input Data (Static)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, stars.size() * sizeof(StarInput), stars.data(), GL_STATIC_DRAW);

    // 2. Allocate Output Buffer (Dynamic - GPU write)
    // Structure: 20 bytes per star (StarRender)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, stars.size() * 20, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void renderStars(const RenderZone& zone, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, float time) {
    if (!computeProgram || maxStars == 0) return;

    // --- 1. COMPUTE PASS (CULLING) ---
    glUseProgram(computeProgram);

    // Bind Buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectBuffer);

    // Reset Indirect Count
    DrawCommand resetCmd = {0, 1, 0, 0};
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawCommand), &resetCmd);
    // Note: We bind to GL_DRAW_INDIRECT_BUFFER for update, but it's bound as SSBO binding 2 for compute

    // Uniforms
    // Use layout(binding) in shader for UBO, so no need to set view/proj manually if UBO is bound globally
    // But we need to ensure UBO is bound to binding 0
    // GlobalUniforms is likely bound to 0 in main.cpp

    glUniform1f(glGetUniformLocation(computeProgram, "screenHeight"), (float)HEIGHT);
    // bulgeRadius not strictly needed for rendering anymore, logic moved to generation

    // Dispatch
    // 256 threads per group
    glDispatchCompute((unsigned int)((maxStars + 255) / 256), 1, 1);

    // Barrier: Wait for shader writes to finish before drawing
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    // --- 2. RENDER PASS ---
    starRenderShader->use();
    starRenderShader->setFloat("screenHeight", (float)HEIGHT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, starSpriteTexture);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(starVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);

    glDrawArraysIndirect(GL_POINTS, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}


// Helper
static uint32_t packColorStar(float r, float g, float b, float a) {
    uint8_t ur = (uint8_t)(glm::clamp(r, 0.0f, 1.0f) * 255.0f);
    uint8_t ug = (uint8_t)(glm::clamp(g, 0.0f, 1.0f) * 255.0f);
    uint8_t ub = (uint8_t)(glm::clamp(b, 0.0f, 1.0f) * 255.0f);
    uint8_t ua = (uint8_t)(glm::clamp(a, 0.0f, 1.0f) * 255.0f);
    return (ua << 24) | (ub << 16) | (ug << 8) | ur;
}

// --- Generation Logic Adapted for Packed StarInput ---
void generateStarField(std::vector<StarInput>& stars, const GalaxyConfig& config) {
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normalDist(0.0f, 1.0f);

    stars.clear();
    stars.reserve(config.numStars);

    for (int i = 0; i < config.numStars; i++) {
        StarInput star;
        float radius, angle, y, velocity;
        float r, g, b, brightness;

        bool inBulge = dist(rng) < 0.15f;

        if (inBulge) {
            float theta = dist(rng) * 2.0f * M_PI;
            float phi = acos(2.0f * dist(rng) - 1.0f);
            float rawRadius = pow(dist(rng), 1.0f / 3.0f) * config.bulgeRadius;

            float x = rawRadius * sin(phi) * cos(theta);
            y = rawRadius * sin(phi) * sin(theta);
            float z = rawRadius * cos(phi);

            radius = sqrt(x * x + z * z);
            angle = atan2(z, x);
            velocity = config.rotationSpeed * 0.5f / (config.bulgeRadius + 1.0f);
        }
        else {
            auto sampleExponentialDiskRadius = [&](float diskScale) -> float {
                float u = dist(rng);
                float r = -diskScale * std::log(1.0f - u + 1e-8f);
                for (int it = 0; it < 10; ++it) {
                    float t = r / diskScale;
                    float expNegT = std::exp(-t);
                    float F = 1.0f - (1.0f + t) * expNegT;
                    float G = F - u;
                    if (std::fabs(G) < 1e-6f) break;
                    float dFdr = (r == 0.0f) ? 0.0f : (r / (diskScale * diskScale)) * expNegT;
                    if (dFdr <= 1e-12f) break;
                    r -= G / dFdr;
                    if (r < 0.0f) { r = 0.0f; break; }
                }
                return r;
            };

            float diskScale = static_cast<float>(config.diskRadius) * 0.25f;
            float rSample = sampleExponentialDiskRadius(diskScale);
            float maxRadius = static_cast<float>(config.diskRadius) * 2.0f;
            if (rSample > maxRadius) rSample = maxRadius;

            float theta = dist(rng) * 2.0f * M_PI;
            float minArmDistance = 1e10f;

            for (int arm = 0; arm < config.numSpiralArms; arm++) {
                float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;
                float spiralTheta = log(rSample / config.bulgeRadius) / config.spiralTightness + armOffset;
                float angleDiff = theta - spiralTheta;
                while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
                minArmDistance = fmin(minArmDistance, fabs(angleDiff * rSample));
            }

            float radiusNorm = rSample / static_cast<float>(config.diskRadius);
            float edgeFactor = (radiusNorm > 1.0f) ? 1.0f : radiusNorm;
            float effectiveArmWidth = config.armWidth * (1.0f + edgeFactor * 1.5f);
            float armProximity = exp(-minArmDistance * minArmDistance / (effectiveArmWidth * effectiveArmWidth));

            float acceptProbability;
            if (rSample > config.diskRadius) {
                 float excessRadius = rSample - config.diskRadius;
                 float fadeScale = config.diskRadius * 0.15f;
                 float outlierFactor = exp(-excessRadius / fadeScale);
                 if (radiusNorm > 1.3f) outlierFactor *= (1.3f/radiusNorm)*(1.3f/radiusNorm);
                 acceptProbability = outlierFactor * 0.08f;
            } else {
                float densityWeight = armProximity * config.armDensityBoost;
                acceptProbability = (1.0f + densityWeight) / (1.0f + config.armDensityBoost);
                if (armProximity < 0.3f) acceptProbability *= 0.2f;
                if (rSample > config.diskRadius * 0.85f) {
                     float t = (config.diskRadius - rSample) / (config.diskRadius * 0.15f);
                     acceptProbability *= (0.5f + 0.5f * t);
                }
            }

            if (dist(rng) > acceptProbability) {
                i--; continue;
            }

            float noiseScale = 15.0f * (1.0f + radiusNorm * 0.8f);
            float noise = normalDist(rng) * noiseScale;
            float radialScatter = normalDist(rng) * 20.0f * radiusNorm * radiusNorm;
            float effectiveRadius = rSample + noise * 0.3f + radialScatter;

            angle = theta;
            radius = effectiveRadius;
            y = normalDist(rng) * config.diskHeight * (1.0f - edgeFactor * 0.5f);
            velocity = config.rotationSpeed * 1.0f / (sqrt(rSample / config.bulgeRadius) * (rSample + 1.0f));
        }

        float typeRoll = dist(rng);
        float cumulative = 0.0f;
        int selectedType = 6;
        for (int t = 0; t < 7; t++) {
            cumulative += starTypes[t].probability;
            if (typeRoll <= cumulative) { selectedType = t; break; }
        }

        r = starTypes[selectedType].r;
        g = starTypes[selectedType].g;
        b = starTypes[selectedType].b;

        float distFromCenter = sqrt(radius * radius + y * y); // Approx
        if (distFromCenter < config.bulgeRadius) {
            brightness = 0.4f + dist(rng) * 0.4f;
        } else {
            brightness = 0.3f + dist(rng) * 0.7f;
            // Recalc arm brightness for final pos
            float minArmDist = 1e10f;
             for (int arm = 0; arm < config.numSpiralArms; arm++) {
                float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;
                float spiralTheta = log(radius / config.bulgeRadius) / config.spiralTightness + armOffset;
                float angleDiff = angle - spiralTheta;
                while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
                minArmDist = fmin(minArmDist, fabs(angleDiff * radius));
            }
            float armBrightness = exp(-minArmDist * minArmDist / (config.armWidth * config.armWidth * 4.0f));
            brightness += armBrightness * 0.3f;
            if (brightness > 1.0f) brightness = 1.0f;
        }

        // --- PACKING ---
        star.radius = radius;
        // SCALE velocity by 1000 to avoid subnormal FP16 precision loss
        star.packedOrbital = glm::packHalf2x16(glm::vec2(angle, velocity * 1000.0f));
        star.packedYBright = glm::packHalf2x16(glm::vec2(y, brightness));
        // Use 1.0 as alpha for now, could store something else
        star.color = packColorStar(r, g, b, 1.0f);

        stars.push_back(star);
    }
}
