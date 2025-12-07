#include "FontRenderer.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace FontRenderer {
    static std::unique_ptr<Shader> textShader;
    static unsigned int textVAO = 0;
    static unsigned int textVBO = 0;
    static unsigned int fontTexture = 0;
    static stbtt_bakedchar cdata[96]; // ASCII 32..126 is 96 chars
    static int screenW, screenH;

    void initFont(int screenWidth, int screenHeight) {
        screenW = screenWidth;
        screenH = screenHeight;

        if (fontTexture != 0) return;

        std::cout << "FontRenderer: initFont(" << screenWidth << ", " << screenHeight << ")" << std::endl;

        // Load Font
        std::cout << "FontRenderer: Allocating buffers..." << std::endl;
        std::vector<unsigned char> ttf_buffer(1 << 20);
        std::vector<unsigned char> temp_bitmap(1024 * 1024);

        // Try to load a standard Windows font
        std::cout << "FontRenderer: Opening font file..." << std::endl;
        FILE* f = fopen("C:/Windows/Fonts/consola.ttf", "rb");
        if (!f) f = fopen("C:/Windows/Fonts/arial.ttf", "rb");
        if (!f) {
            std::cerr << "Failed to open font file!" << std::endl;
            return;
        }

        std::cout << "FontRenderer: Reading font file..." << std::endl;
        fread(ttf_buffer.data(), 1, 1 << 20, f);
        fclose(f);

        std::cout << "FontRenderer: Baking font bitmap..." << std::endl;
        // Bake at 64.0f (4x original resolution) for sharpness
        stbtt_BakeFontBitmap(ttf_buffer.data(), 0, 64.0f, temp_bitmap.data(), 1024, 1024, 32, 96, cdata);

        // Texture
        std::cout << "FontRenderer: Creating texture..." << std::endl;
        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        // Use GL_RED for single channel
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Shader
        std::cout << "FontRenderer: Creating shader..." << std::endl;
        textShader = std::make_unique<Shader>("assets/shaders/text.vert", "assets/shaders/text.frag");

        // VAO/VBO
        std::cout << "FontRenderer: Creating VAO/VBO..." << std::endl;
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);
        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        // Dynamic draw for changing text - size will be set per frame
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 100, NULL, GL_DYNAMIC_DRAW); // Initial allocation

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        std::cout << "FontRenderer: Initialized." << std::endl;
    }

    void renderText(const std::string& text, float x, float y, float scale,
        float r, float g, float b, float a) {
        if (fontTexture == 0) return;

        textShader->use();
        textShader->setVec3("textColor", r, g, b);

        // Projection (Top-Left Origin)
        glm::mat4 projection = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
        textShader->setMat4("projection", glm::value_ptr(projection));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        textShader->setInt("text", 0);

        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);

        std::vector<float> vertices;
        vertices.reserve(text.length() * 6 * 4);

        // Use a relative cursor starting at 0,0 to get unscaled offsets
        float currentX = 0;
        float currentY = 0;

        // Original size was 16.0f, new is 64.0f.
        // We must scale down by 0.25 to match the expected visual size of scale=1.0.
        float finalScale = scale * (16.0f / 64.0f);

        for (char c : text) {
            if (c < 32 || c >= 128) continue;

            stbtt_aligned_quad q;
            // We pass relative coordinates to GetBakedQuad.
            // It updates currentX/currentY for the NEXT character.
            stbtt_GetBakedQuad(cdata, 1024, 1024, c - 32, &currentX, &currentY, &q, 1);

            // Now we apply the scale and the actual start position (x,y)
            // q contains offsets from (0,0) + glyph size.
            // Transformed = Start + Offset * Scale
            float x0 = x + q.x0 * finalScale;
            float y0 = y + q.y0 * finalScale;
            float x1 = x + q.x1 * finalScale;
            float y1 = y + q.y1 * finalScale;

            // 6 vertices per quad (2 triangles)
            // Triangle 1
            vertices.push_back(x0); vertices.push_back(y0); vertices.push_back(q.s0); vertices.push_back(q.t0);
            vertices.push_back(x0); vertices.push_back(y1); vertices.push_back(q.s0); vertices.push_back(q.t1);
            vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(q.s1); vertices.push_back(q.t1);

            // Triangle 2
            vertices.push_back(x0); vertices.push_back(y0); vertices.push_back(q.s0); vertices.push_back(q.t0);
            vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(q.s1); vertices.push_back(q.t1);
            vertices.push_back(x1); vertices.push_back(y0); vertices.push_back(q.s1); vertices.push_back(q.t0);
        }

        // Upload all vertices at once
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

        // Draw all triangles
        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    float getTextWidth(const std::string& text, float scale) {
        float x = 0;
        float y = 0;
        for (char c : text) {
            if (c < 32 || c >= 128) continue;
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 1024, 1024, c - 32, &x, &y, &q, 1);
        }
        // Adjust scale to match original 16px baseline
        return x * scale * (16.0f / 64.0f);
    }

    void cleanup() {
        if (textVAO) glDeleteVertexArrays(1, &textVAO);
        if (textVBO) glDeleteBuffers(1, &textVBO);
        if (fontTexture) glDeleteTextures(1, &fontTexture);
        textShader.reset();
    }
}
