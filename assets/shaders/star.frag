#version 430 core
in vec4 vColor; // rgb: color, a: brightness
out vec4 FragColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);

    if (distSq > 0.25) discard;

    // Core: Sharp, high intensity
    float core = exp(-distSq * 80.0);

    // Halo: Wide, faint glow
    float halo = exp(-distSq * 8.0) * 0.4;

    // Combine
    float alpha = core + halo;

    // Apply brightness from vertex shader (vColor.a)
    // This ensures dim stars are actually fainter, not just smaller.
    FragColor = vec4(vColor.rgb, alpha * vColor.a);
}
