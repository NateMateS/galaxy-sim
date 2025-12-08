#version 430 core
in vec4 vColor; // rgb: color, a: brightness
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
    // Sample pre-computed sprite
    // gl_PointCoord is 0..1, which matches texture UVs perfectly
    float alpha = texture(spriteTexture, gl_PointCoord).a;

    // Apply brightness from vertex shader (vColor.a)
    FragColor = vec4(vColor.rgb, alpha * vColor.a);
}
