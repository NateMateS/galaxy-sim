#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D planetTexture;
uniform vec3 lightPos; // Sun position
uniform vec3 atmosphereColor;

layout (std140) uniform GlobalUniforms {
    mat4 view;
    mat4 projection;
    vec4 viewPos; // .xyz is position
    float time;
};

void main()
{
    // 1. Diffuse Lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    // 2. Specular (Water Only - simplifed)
    vec3 viewDir = normalize(viewPos.xyz - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec4 texColor = texture(planetTexture, TexCoords);

    // Assume Blue-ish pixels are water for specular mask
    // Heuristic: Blue channel is significantly dominant over Red and Green
    float blueDominance = 0.1; // Threshold for water detection
    float isWater = step(texColor.r + blueDominance, texColor.b) * step(texColor.g + blueDominance, texColor.b);

    float specularStrength = mix(0.0, 0.8, isWater); // 0.0 for land, 0.8 for water
    vec3 specular = specularStrength * spec * vec3(1.0); // White specular highlight

    // 3. Ambient
    vec3 ambient = 0.05 * texColor.rgb; // Space is dark
    vec3 diffuse = diff * texColor.rgb;

    // 4. Rim Lighting (Atmosphere)
    // Dot product of Normal and View Direction gives edge factor
    float rimFactor = 1.0 - max(dot(viewDir, norm), 0.0);
    rimFactor = pow(rimFactor, 4.0); // Sharp rim
    vec3 rim = rimFactor * atmosphereColor * 1.5;

    // Shadow side shouldn't have (much) rim light from atmosphere scattering sun
    // But usually atmosphere glows on the sun side.
    // Let's simplify: Atmosphere glows everywhere but brighter on sun side.
    float sunFacing = max(dot(norm, lightDir), 0.0) * 0.5 + 0.5;
    rim *= sunFacing;

    vec3 result = ambient + diffuse + specular + rim;
    FragColor = vec4(result, 1.0);
}
