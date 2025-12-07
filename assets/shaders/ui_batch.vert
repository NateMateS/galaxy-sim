#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;
layout (location = 3) in float aMode; // 0.0 for Text, 1.0 for Rect

out vec2 TexCoords;
out vec4 Color;
out float Mode;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoords = aTexCoords;
    Color = aColor;
    Mode = aMode;
}
