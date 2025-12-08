#pragma once
#include <vector>
#include <glad/glad.h>

class TextureGenerator {
public:
    // Generates a seamless noise texture
    static unsigned int GenerateNoiseTexture(int width, int height, float scale, float persistence, int octaves, unsigned int seed);

    // Generates a spherical map for planets (e.g. Continents)
    static unsigned int GeneratePlanetTexture(int width, int height, unsigned int seed, float waterLevel, float r1, float g1, float b1, float r2, float g2, float b2);

    // Generates a turbulent star surface texture
    static unsigned int GenerateSunTexture(int width, int height, unsigned int seed);

    // Generates a simple glow sprite for stars/particles
    static unsigned int GenerateGlowSprite(int width, int height);

    // Frees the static compute program
    static void Cleanup();
};
