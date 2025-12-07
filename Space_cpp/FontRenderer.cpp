#include "FontRenderer.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace FontRenderer {
    static unsigned int fontTexture = 0;
    static stbtt_bakedchar cdata[96]; // ASCII 32..126 is 96 chars
    static int screenW, screenH;

    void initFont(int screenWidth, int screenHeight) {
        screenW = screenWidth;
        screenH = screenHeight;

        if (fontTexture != 0) return;

        // Load Font
        std::vector<unsigned char> ttf_buffer(1 << 20);
        std::vector<unsigned char> temp_bitmap(1024 * 1024);

        FILE* f = fopen("C:/Windows/Fonts/consola.ttf", "rb");
        if (!f) f = fopen("C:/Windows/Fonts/arial.ttf", "rb");
        if (!f) {
            std::cerr << "Failed to open font file!" << std::endl;
            return;
        }

        fread(ttf_buffer.data(), 1, 1 << 20, f);
        fclose(f);

        stbtt_BakeFontBitmap(ttf_buffer.data(), 0, 32.0f, temp_bitmap.data(), 1024, 1024, 32, 96, cdata);

        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap.data());
        // No mipmaps for sharp UI text
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    void appendText(const std::string& text, float x, float y, float scale,
        float r, float g, float b, float a,
        std::vector<UIVertex>& buffer) {

        float currentX = 0;
        float currentY = 0;
        float finalScale = scale * (16.0f / 32.0f);

        for (char c : text) {
            if (c < 32 || c >= 128) continue;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 1024, 1024, c - 32, &currentX, &currentY, &q, 1);

            // Pixel snapping for sharp text
            float x0 = std::floor(x + q.x0 * finalScale + 0.5f);
            float y0 = std::floor(y + q.y0 * finalScale + 0.5f);
            float x1 = std::floor(x + q.x1 * finalScale + 0.5f);
            float y1 = std::floor(y + q.y1 * finalScale + 0.5f);

            // 6 vertices per quad (2 triangles)
            // Mode 0.0 = Text

            // Triangle 1
            buffer.push_back({x0, y0, q.s0, q.t0, r, g, b, a, 0.0f});
            buffer.push_back({x0, y1, q.s0, q.t1, r, g, b, a, 0.0f});
            buffer.push_back({x1, y1, q.s1, q.t1, r, g, b, a, 0.0f});

            // Triangle 2
            buffer.push_back({x0, y0, q.s0, q.t0, r, g, b, a, 0.0f});
            buffer.push_back({x1, y1, q.s1, q.t1, r, g, b, a, 0.0f});
            buffer.push_back({x1, y0, q.s1, q.t0, r, g, b, a, 0.0f});
        }
    }

    float getTextWidth(const std::string& text, float scale) {
        float x = 0;
        float y = 0;
        for (char c : text) {
            if (c < 32 || c >= 128) continue;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 1024, 1024, c - 32, &x, &y, &q, 1);
        }
        return x * scale * (16.0f / 32.0f);
    }

    unsigned int getFontTexture() {
        return fontTexture;
    }

    void cleanup() {
        if (fontTexture) glDeleteTextures(1, &fontTexture);
    }
}
