#include "TextureGenerator.h"
#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Helper to manage the Compute Shader
static unsigned int computeProgram = 0;

static void InitComputeShader() {
    if (computeProgram != 0) return;

    std::string code;
    std::ifstream file("assets/shaders/texture_gen.comp");
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open assets/shaders/texture_gen.comp" << std::endl;
        return;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    code = ss.str();
    const char* cCode = code.c_str();

    unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &cCode, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n" << infoLog << std::endl;
        return;
    }

    computeProgram = glCreateProgram();
    glAttachShader(computeProgram, shader);
    glLinkProgram(computeProgram);

    glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(computeProgram, 1024, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return;
    }

    glDeleteShader(shader);
}

static void DispatchGen(unsigned int textureID, int width, int height, int mode, float scale, int octaves, float persistence, int seed, float waterLevel = 0.0f, float r1=0, float g1=0, float b1=0, float r2=0, float g2=0, float b2=0) {
    InitComputeShader();
    if (computeProgram == 0) return;

    glUseProgram(computeProgram);

    // Bind image
    glBindImageTexture(0, textureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    glUniform1i(glGetUniformLocation(computeProgram, "mode"), mode);
    glUniform1i(glGetUniformLocation(computeProgram, "width"), width);
    glUniform1i(glGetUniformLocation(computeProgram, "height"), height);
    glUniform1f(glGetUniformLocation(computeProgram, "scale"), scale);
    glUniform1i(glGetUniformLocation(computeProgram, "octaves"), octaves);
    glUniform1f(glGetUniformLocation(computeProgram, "persistence"), persistence);
    glUniform1i(glGetUniformLocation(computeProgram, "seed"), seed);

    if (mode == 1) {
        glUniform1f(glGetUniformLocation(computeProgram, "waterLevel"), waterLevel);
        glUniform3f(glGetUniformLocation(computeProgram, "color1"), r1, g1, b1);
        glUniform3f(glGetUniformLocation(computeProgram, "color2"), r2, g2, b2);
    }

    // Dispatch
    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

unsigned int TextureGenerator::GenerateNoiseTexture(int width, int height, float scale, float persistence, int octaves, unsigned int seed) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    // Use mutable storage (glTexImage2D) to allow for subsequent mipmap generation.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    DispatchGen(textureID, width, height, 0, scale, octaves, persistence, seed);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glGenerateMipmap(GL_TEXTURE_2D);

    return textureID;
}

unsigned int TextureGenerator::GenerateGlowSprite(int width, int height) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Clamp to edge to avoid artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Mode 3: Glow Sprite
    DispatchGen(textureID, width, height, 3, 1.0f, 1, 0.0f, 0);

    // Mipmaps disabled to avoid synchronization issues with compute shader
    glBindTexture(GL_TEXTURE_2D, textureID);

    return textureID;
}

void TextureGenerator::Cleanup() {
    if (computeProgram != 0) {
        glDeleteProgram(computeProgram);
        computeProgram = 0;
    }
}

unsigned int TextureGenerator::GeneratePlanetTexture(int width, int height, unsigned int seed, float waterLevel, float r1, float g1, float b1, float r2, float g2, float b2) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    DispatchGen(textureID, width, height, 1, 2.0f, 6, 0.5f, seed, waterLevel, r1, g1, b1, r2, g2, b2);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glGenerateMipmap(GL_TEXTURE_2D);

    return textureID;
}

unsigned int TextureGenerator::GenerateSunTexture(int width, int height, unsigned int seed) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    DispatchGen(textureID, width, height, 2, 3.0f, 8, 0.6f, seed);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glGenerateMipmap(GL_TEXTURE_2D);

    return textureID;
}
