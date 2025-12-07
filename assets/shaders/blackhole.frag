#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;

uniform sampler2D noiseTexture;
uniform vec3 centerPos;
uniform float innerRadius;
uniform float outerRadius;

layout (std140) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 viewPos;
    float time;
};

void main()
{
    // Center UVs to (-1, 1)
    vec2 uv = TexCoords * 2.0 - 1.0;
    float dist = length(uv);

    // Discard if outside disk
    if (dist > 1.0 || dist < 0.15) discard; // 0.15 is event horizon hole

    // Calculate rotation
    float angle = atan(uv.y, uv.x);
    float rotationSpeed = 1.0 / (dist * dist + 0.1); // Faster near center
    float currentAngle = angle + time * rotationSpeed;

    // Sample noise with rotation
    // Map (dist, angle) to texture coordinates
    vec2 polarUV = vec2(currentAngle * 2.0, dist * 0.5); // Stretch noise along ring

    vec3 noise = texture(noiseTexture, polarUV).rgb;

    // Color Mapping (Accretion Disk colors: Orange/White hot)
    vec3 color = vec3(1.0, 0.8, 0.4) * noise.r;

    // Hotter near center
    color += vec3(0.5, 0.2, 0.0) * (1.0 - dist) * 2.0;

    // Doppler Beaming Simulation
    // Assume disk rotates counter-clockwise.
    // Left side (x < 0) moves towards camera? (Depends on view, simplifying assumption)
    // Let's make left side brighter/bluer, right side redder/dimmer
    float doppler = -uv.x * rotationSpeed * 0.3;

    // Apply doppler shift to color
    // Blue shift (positive doppler): More Blue, brighter
    // Red shift (negative doppler): More Red, dimmer
    vec3 dopplerColor = color;
    dopplerColor.b += doppler;
    dopplerColor.r -= doppler;
    dopplerColor *= (1.0 + doppler); // Intensity shift

    // Event Horizon Glow (Photon Sphere)
    // Sharp ring at the inner edge
    float photonRing = smoothstep(0.15, 0.18, dist) * smoothstep(0.21, 0.18, dist);
    vec3 photonColor = vec3(1.0, 1.0, 1.0) * photonRing * 5.0; // Super bright

    vec3 finalColor = dopplerColor * 2.0 + photonColor;

    // Soft fade at outer edge
    float alpha = smoothstep(1.0, 0.8, dist);

    FragColor = vec4(finalColor, alpha);
}
