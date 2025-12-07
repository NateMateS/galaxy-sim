#pragma once
#include <string>
#include <vector>

struct UIVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
    float mode; // 0.0 for Text, 1.0 for Rect
};

namespace FontRenderer {
    void initFont(int screenWidth, int screenHeight);

    // Batch mode
    void appendText(const std::string& text, float x, float y, float scale,
        float r, float g, float b, float a,
        std::vector<UIVertex>& buffer);

    float getTextWidth(const std::string& text, float scale);

    unsigned int getFontTexture();

    void cleanup();
}
