#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 Color;
in float Mode;

uniform sampler2D textTexture;

void main()
{
    if (Mode < 0.5) {
        // Text Mode: Sample Red channel as alpha
        float alpha = texture(textTexture, TexCoords).r;
        FragColor = vec4(Color.rgb, Color.a * alpha);
    } else {
        // Rect Mode: Just use color
        FragColor = Color;
    }
}
