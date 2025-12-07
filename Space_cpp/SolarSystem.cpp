#include "SolarSystem.h"
#include "UI.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const PlanetData PLANET_DATA[NUM_PLANETS] = {
    {"Mercury", 0.39, 0.383, 0.7f, 0.7f, 0.7f},
    {"Venus", 0.72, 0.949, 0.9f, 0.8f, 0.6f},
    {"Earth", 1.00, 1.000, 0.3f, 0.5f, 0.8f},
    {"Mars", 1.52, 0.532, 0.8f, 0.4f, 0.3f},
    {"Jupiter", 5.20, 11.21, 0.9f, 0.8f, 0.6f},
    {"Saturn", 9.54, 9.45, 0.9f, 0.9f, 0.7f},
    {"Uranus", 19.2, 4.01, 0.6f, 0.8f, 0.9f},
    {"Neptune", 30.1, 3.88, 0.4f, 0.5f, 0.9f}};

SolarSystem solarSystem = {0.0, 0.0, 0.0, false};
Sun sun = {0.0, 0.0, 0.0, 2.0};
std::vector<Planet> planets;

// Render Resources
static unsigned int sphereVAO = 0;
static unsigned int sphereIndexCount = 0;
static unsigned int orbitVAO = 0;
static unsigned int orbitPointCount = 0;

void initSolarSystemRender() {
    if (sphereVAO != 0) return;

    // --- Sphere (Sun/Planets) ---
    glGenVertexArrays(1, &sphereVAO);
    unsigned int vbo, ebo;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    std::vector<float> data;
    std::vector<unsigned int> indices;

    const unsigned int X_SEGMENTS = 64;
    const unsigned int Y_SEGMENTS = 64;
    const float PI = 3.14159265359f;

    for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            // Pos
            data.push_back(xPos);
            data.push_back(yPos);
            data.push_back(zPos);
            // Normal
            data.push_back(xPos);
            data.push_back(yPos);
            data.push_back(zPos);
            // UV
            data.push_back(xSegment);
            data.push_back(ySegment);
        }
    }

    bool oddRow = false;
    for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
        if (!oddRow) {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
            }
        } else {
            for (int x = X_SEGMENTS; x >= 0; --x) {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
            }
        }
        oddRow = !oddRow;
    }
    sphereIndexCount = (unsigned int)indices.size();

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    float stride = (3 + 3 + 2) * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (int)stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (int)stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (int)stride, (void*)(6 * sizeof(float)));

    // --- Orbits (Unit Circle) ---
    glGenVertexArrays(1, &orbitVAO);
    unsigned int orbitVBO;
    glGenBuffers(1, &orbitVBO);

    std::vector<float> orbitData;
    orbitPointCount = 128;
    for (unsigned int i = 0; i < orbitPointCount; ++i) {
        float angle = (float)i / (float)orbitPointCount * 2.0f * PI;
        orbitData.push_back(cos(angle)); // x
        orbitData.push_back(0.0f);       // y
        orbitData.push_back(sin(angle)); // z
    }

    glBindVertexArray(orbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitData.size() * sizeof(float), orbitData.data(), GL_STATIC_DRAW);

    // Position attribute (Location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void cleanupSolarSystemRender() {
    if (sphereVAO != 0) {
        glDeleteVertexArrays(1, &sphereVAO);
        sphereVAO = 0;
    }
    if (orbitVAO != 0) {
        glDeleteVertexArrays(1, &orbitVAO);
        orbitVAO = 0;
    }
}

RenderZone calculateRenderZone(const Camera &camera)
{
    RenderZone zone;
    zone.zoomLevel = camera.zoomLevel;
    zone.distanceFromSystem = 0.0;

    const double GALAXY_ZOOM_MAX = 0.1;
    const double SYSTEM_ZOOM_MIN = 100.0;

    if (camera.zoomLevel < GALAXY_ZOOM_MAX)
    {
        zone.solarSystemScaleMultiplier = 1.0;
        zone.starBrightnessFade = 1.0;
        zone.renderOrbits = false;
    }
    else if (camera.zoomLevel < SYSTEM_ZOOM_MIN)
    {
        double t = (camera.zoomLevel - GALAXY_ZOOM_MAX) / (SYSTEM_ZOOM_MIN - GALAXY_ZOOM_MAX);
        t = t * t * t; // Cubic easing

        zone.solarSystemScaleMultiplier = 1.0 + (g_currentSolarSystemScale - 1.0) * t;
        zone.starBrightnessFade = 1.0;
        zone.renderOrbits = false;
    }
    else
    {
        zone.solarSystemScaleMultiplier = g_currentSolarSystemScale;
        zone.starBrightnessFade = 1.0;
        zone.renderOrbits = true;
    }

    return zone;
}

void generateSolarSystem()
{
    std::cout << "Generating solar system..." << std::endl;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // radius 200-600 to avoid bulge and edge
    double radius = 200.0 + dist(rng) * 400.0;
    double angle = dist(rng) * 2.0 * M_PI;
    double verticalOffset = (dist(rng) - 0.5) * 20.0;

    solarSystem.centerX = radius * cos(angle);
    solarSystem.centerY = verticalOffset;
    solarSystem.centerZ = radius * sin(angle);
    solarSystem.isGenerated = true;

    sun.x = solarSystem.centerX;
    sun.y = solarSystem.centerY;
    sun.z = solarSystem.centerZ;
    sun.radius = 2.0;

    planets.clear();
    for (int i = 0; i < NUM_PLANETS; i++)
    {
        Planet planet;
        planet.orbitRadius = PLANET_DATA[i].orbitRadius * 0.15;
        planet.radius = PLANET_DATA[i].radius * 0.01;
        planet.r = PLANET_DATA[i].r;
        planet.g = PLANET_DATA[i].g;
        planet.b = PLANET_DATA[i].b;
        planet.angle = dist(rng) * 2.0 * M_PI;
        planet.orbitalSpeed = 0.0005 / sqrt(planet.orbitRadius);

        planet.x = sun.x + planet.orbitRadius * cos(planet.angle);
        planet.y = sun.y;
        planet.z = sun.z + planet.orbitRadius * sin(planet.angle);

        planets.push_back(planet);
    }

    std::cout << "Solar system at (" << solarSystem.centerX << ", "
              << solarSystem.centerY << ", " << solarSystem.centerZ << ")" << std::endl;
}

void updatePlanets(double deltaTime)
{
    for (auto &planet : planets)
    {
        planet.angle += planet.orbitalSpeed * deltaTime;
        while (planet.angle > 2.0 * M_PI)
            planet.angle -= 2.0 * M_PI;
        while (planet.angle < 0.0)
            planet.angle += 2.0 * M_PI;

        planet.x = sun.x + planet.orbitRadius * cos(planet.angle);
        planet.z = sun.z + planet.orbitRadius * sin(planet.angle);
    }
}

void renderSolarSystem(const RenderZone &zone, const Camera& camera,
    unsigned int sunTexture, unsigned int planetTexture,
    Shader* sunShader, Shader* planetShader, Shader* orbitShader)
{
    if (sphereVAO == 0) initSolarSystemRender();

    // --- Render Sun ---
    sunShader->use();

    float sunRadius = 0.01f;
    if (zone.zoomLevel > 1000.0) sunRadius = 0.05f;
    else if (zone.zoomLevel > 500.0) sunRadius = 0.04f;
    else if (zone.zoomLevel > 100.0) sunRadius = 0.03f;
    else if (zone.zoomLevel > 10.0) sunRadius = 0.02f;
    else if (zone.zoomLevel > 1.0) sunRadius = 0.015f;

    // Model Matrix: Translate * Scale
    glm::mat4 sunModel = glm::mat4(1.0f);
    sunModel = glm::translate(sunModel, glm::vec3((float)sun.x, (float)sun.y, (float)sun.z));
    sunModel = glm::scale(sunModel, glm::vec3(sunRadius));

    sunShader->setMat4("model", glm::value_ptr(sunModel));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sunTexture);
    sunShader->setInt("sunTexture", 0);

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLE_STRIP, sphereIndexCount, GL_UNSIGNED_INT, 0);

    // --- Render Planets ---
    planetShader->use();
    planetShader->setVec3("lightPos", (float)sun.x, (float)sun.y, (float)sun.z);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetTexture);
    planetShader->setInt("planetTexture", 0);

    for (const auto &planet : planets)
    {
        float planetRadius = 0.002f;
        if (zone.zoomLevel > 1000.0) planetRadius = 0.003f;
        else if (zone.zoomLevel > 500.0) planetRadius = 0.003f;
        else if (zone.zoomLevel > 100.0) planetRadius = 0.0025f;
        else if (zone.zoomLevel > 50.0) planetRadius = 0.002f;
        else if (zone.zoomLevel > 10.0) planetRadius = 0.002f;

        glm::mat4 planetModel = glm::mat4(1.0f);
        planetModel = glm::translate(planetModel, glm::vec3((float)planet.x, (float)planet.y, (float)planet.z));
        planetModel = glm::scale(planetModel, glm::vec3(planetRadius));

        planetShader->setMat4("model", glm::value_ptr(planetModel));

        // Simple Atmosphere Color based on planet color
        planetShader->setVec3("atmosphereColor", planet.r * 0.5f, planet.g * 0.5f, planet.b * 0.8f);

        glDrawElements(GL_TRIANGLE_STRIP, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    // --- Render Orbits (Modern GL) ---
    if (zone.renderOrbits && orbitShader)
    {
        orbitShader->use();
        orbitShader->setVec3("color", 0.2f, 0.2f, 0.2f); // Dim orbit lines

        glBindVertexArray(orbitVAO);

        for (const auto &planet : planets)
        {
             // Model Matrix for Orbit:
             // Translate to Sun Position -> Scale by Orbit Radius
             glm::mat4 orbitModel = glm::mat4(1.0f);
             orbitModel = glm::translate(orbitModel, glm::vec3((float)sun.x, (float)sun.y, (float)sun.z));
             // Scale (x, y, z) = (radius, 1, radius) because orbit is on XZ plane
             orbitModel = glm::scale(orbitModel, glm::vec3((float)planet.orbitRadius, 1.0f, (float)planet.orbitRadius));

             orbitShader->setMat4("model", glm::value_ptr(orbitModel));
             glDrawArrays(GL_LINE_LOOP, 0, orbitPointCount);
        }
        glBindVertexArray(0);
    }

    glUseProgram(0);
}
