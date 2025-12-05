#version 330 core
in vec3 Color;
out vec4 FragColor;

void main() {
    // gl_PointCoord coords are (0,0) top-left to (1,1) bottom-right
    // Calculate distance squared from center (0.5, 0.5) for performance
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);

    // Discard pixels outside the circle radius (0.5^2 = 0.25)
    if (distSq > 0.25) discard;

    // DUAL-LOBE OPTICAL MODEL
    // 1. The Core: A very sharp, high-intensity Gaussian.
    //    This represents the resolved star disk/diffraction limit.
    //    It ensures stars look like points, not blobs.
    float core = exp(-distSq * 80.0);

    // 2. The Halo: A wide, faint Gaussian.
    //    This represents atmospheric/lens scattering (Bloom).
    //    It provides the "glow" without obscuring detail.
    float halo = exp(-distSq * 8.0) * 0.4;

    // Combine them. The core dominates the center, halo dominates the edge.
    float alpha = core + halo;

    // Apply the alpha to the color
    FragColor = vec4(Color, alpha);
}
