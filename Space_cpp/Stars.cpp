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

    // Attrib 0: Pos + Doppler (vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 32, (void*)0); // 8 floats stride

    // Attrib 1: Color + Size (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);

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
    // Structure: vec4 pos_doppler, vec4 color_size (32 bytes per star)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, stars.size() * 32, NULL, GL_DYNAMIC_DRAW);

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
    starRenderShader->setMat4("view", glm::value_ptr(view));
    starRenderShader->setMat4("projection", glm::value_ptr(projection));
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


// --- Generation Logic Adapted for StarInput ---
void generateStarField(std::vector<StarInput>& stars, const GalaxyConfig& config) {
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::normal_distribution<float> normalDist(0.0f, 1.0f);

    stars.clear();
    stars.reserve(config.numStars);

    for (int i = 0; i < config.numStars; i++) {
        StarInput star;
        float x, z; // Temp for calc

        bool inBulge = dist(rng) < 0.15f;

        if (inBulge) {
            float theta = dist(rng) * 2.0f * M_PI;
            float phi = acos(2.0f * dist(rng) - 1.0f);
            float radius = pow(dist(rng), 1.0f / 3.0f) * config.bulgeRadius;

            x = radius * sin(phi) * cos(theta);
            star.y = radius * sin(phi) * sin(theta);
            z = radius * cos(phi);

            star.radius = sqrt(x * x + z * z);
            star.angle = atan2(z, x);
            star.velocity = config.rotationSpeed * 0.5f / (config.bulgeRadius + 1.0f);
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
            float radius = sampleExponentialDiskRadius(diskScale);
            float maxRadius = static_cast<float>(config.diskRadius) * 2.0f;
            if (radius > maxRadius) radius = maxRadius;

            float theta = dist(rng) * 2.0f * M_PI;
            float minArmDistance = 1e10f;

            for (int arm = 0; arm < config.numSpiralArms; arm++) {
                float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;
                float spiralTheta = log(radius / config.bulgeRadius) / config.spiralTightness + armOffset;
                float angleDiff = theta - spiralTheta;
                while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
                minArmDistance = fmin(minArmDistance, fabs(angleDiff * radius));
            }

            float radiusNorm = radius / static_cast<float>(config.diskRadius);
            float edgeFactor = (radiusNorm > 1.0f) ? 1.0f : radiusNorm;
            float effectiveArmWidth = config.armWidth * (1.0f + edgeFactor * 1.5f);
            float armProximity = exp(-minArmDistance * minArmDistance / (effectiveArmWidth * effectiveArmWidth));

            float acceptProbability;
            if (radius > config.diskRadius) {
                 float excessRadius = radius - config.diskRadius;
                 float fadeScale = config.diskRadius * 0.15f;
                 float outlierFactor = exp(-excessRadius / fadeScale);
                 if (radiusNorm > 1.3f) outlierFactor *= (1.3f/radiusNorm)*(1.3f/radiusNorm);
                 acceptProbability = outlierFactor * 0.08f;
            } else {
                float densityWeight = armProximity * config.armDensityBoost;
                acceptProbability = (1.0f + densityWeight) / (1.0f + config.armDensityBoost);
                if (armProximity < 0.3f) acceptProbability *= 0.2f;
                if (radius > config.diskRadius * 0.85f) {
                     float t = (config.diskRadius - radius) / (config.diskRadius * 0.15f);
                     acceptProbability *= (0.5f + 0.5f * t);
                }
            }

            if (dist(rng) > acceptProbability) {
                i--; continue;
            }

            float noiseScale = 15.0f * (1.0f + radiusNorm * 0.8f);
            float noise = normalDist(rng) * noiseScale;
            float radialScatter = normalDist(rng) * 20.0f * radiusNorm * radiusNorm;
            float effectiveRadius = radius + noise * 0.3f + radialScatter;

            star.angle = theta;
            star.radius = effectiveRadius;
            star.y = normalDist(rng) * config.diskHeight * (1.0f - edgeFactor * 0.5f);
            star.velocity = config.rotationSpeed * 1.0f / (sqrt(radius / config.bulgeRadius) * (radius + 1.0f));
        }

        float typeRoll = dist(rng);
        float cumulative = 0.0f;
        int selectedType = 6;
        for (int t = 0; t < 7; t++) {
            cumulative += starTypes[t].probability;
            if (typeRoll <= cumulative) { selectedType = t; break; }
        }

        star.r = starTypes[selectedType].r;
        star.g = starTypes[selectedType].g;
        star.b = starTypes[selectedType].b;

        float distFromCenter = sqrt(star.radius * star.radius + star.y * star.y); // Approx
        if (distFromCenter < config.bulgeRadius) {
            star.brightness = 0.4f + dist(rng) * 0.4f;
        } else {
            star.brightness = 0.3f + dist(rng) * 0.7f;
            // Recalc arm brightness for final pos
            float minArmDist = 1e10f;
             for (int arm = 0; arm < config.numSpiralArms; arm++) {
                float armOffset = (arm * 2.0f * M_PI) / config.numSpiralArms;
                float spiralTheta = log(star.radius / config.bulgeRadius) / config.spiralTightness + armOffset;
                float angleDiff = star.angle - spiralTheta;
                while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
                minArmDist = fmin(minArmDist, fabs(angleDiff * star.radius));
            }
            float armBrightness = exp(-minArmDist * minArmDist / (config.armWidth * config.armWidth * 4.0f));
            star.brightness += armBrightness * 0.3f;
            if (star.brightness > 1.0f) star.brightness = 1.0f;
        }

        stars.push_back(star);
    }
}
