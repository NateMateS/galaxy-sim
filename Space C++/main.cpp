#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <random>
#include <memory>
#include <glm/glm.hpp>

#include "Window.h"
#include "Camera.h"
#include "Stars.h"
#include "SolarSystem.h"
#include "BlackHole.h"
#include "GalacticGas.h"
#include "Input.h"
#include "UI.h"
#include "PostProcessor.h"
#include "TextureGenerator.h"
#include "Shader.h"

int WIDTH = 1280;
int HEIGHT = 720;

// Render Resources
std::unique_ptr<PostProcessor> postProcessor;
std::unique_ptr<Shader> planetShader;
std::unique_ptr<Shader> sunShader;
std::unique_ptr<Shader> blackHoleShader;
std::unique_ptr<Shader> gasShader;
std::unique_ptr<Shader> orbitShader;

unsigned int sunTexture = 0;
unsigned int planetTexture = 0;
unsigned int noiseTexture = 0;

GalaxyConfig createDefaultGalaxyConfig() {
	GalaxyConfig config;
	config.numStars = 1000000;
	config.numSpiralArms = 2;
	config.spiralTightness = 0.3;
	config.armWidth = 60.0;
	config.diskRadius = 800.0;
	config.bulgeRadius = 150.0;
	config.diskHeight = 50.0;
	config.bulgeHeight = 100.0;
	config.armDensityBoost = 10.0;

	std::random_device rd;
	config.seed = rd();

	config.rotationSpeed = 1.0;

	std::cout << "Galaxy seed: " << config.seed << std::endl;

	return config;
}

BlackHoleConfig createDefaultBlackHoleConfig() {
	BlackHoleConfig config;
	config.enableSupermassive = true;

	return config;
}

void render(const std::vector<Star>& stars, const std::vector<BlackHole>& blackHoles,
	const std::vector<GasVertex>& darkGas, const std::vector<GasVertex>& luminousGas,
    const Camera& camera, UIState& uiState) {
    if (!postProcessor) {
        fprintf(stderr, "FATAL: postProcessor is NULL in render!\n");
        return;
    }

    // 1. Render to HDR/MSAA Framebuffer
    postProcessor->BeginRender();

    glm::mat4 view, projection;
	getCameraMatrices(camera, WIDTH, HEIGHT, solarSystem, view, projection);

	RenderZone zone = calculateRenderZone(camera);

    // Render Order: Opaque -> Transparent

	if (solarSystem.isGenerated) {
		if (!sunShader) fprintf(stderr, "sunShader is NULL\n");
		if (!planetShader) fprintf(stderr, "planetShader is NULL\n");
		if (!orbitShader) fprintf(stderr, "orbitShader is NULL\n");

		renderSolarSystem(zone, camera, view, projection, sunTexture, planetTexture, sunShader.get(), planetShader.get(), orbitShader.get());
	}

    // Copy Depth to break Feedback Loop (Read Copy, Write Original)
    postProcessor->CopyDepth();

    // Transparent / Additive
    // Stars (Additive)
	renderStars(stars, zone, view, projection);

    // Gas (Blend with Soft Particles)
    // We use the MSAA depth COPY directly via sampler2DMS
    renderGalacticGas(darkGas, luminousGas, (float)glfwGetTime(), postProcessor->MSAADepthCopyTexture, (float)WIDTH, (float)HEIGHT, zone, view, projection, gasShader.get());

    // Black Holes (Blend)
	renderBlackHoles(blackHoles, zone, camera, view, projection, noiseTexture, blackHoleShader.get());

    // 2. Post-Processing (Bloom, Tone Mapping) -> Screen
    postProcessor->EndRender();

    // UI rendered on top of everything (Post-process result is just a quad)
 	renderUI(uiState, WIDTH, HEIGHT);
}

int main() {
	WindowConfig windowConfig = { WIDTH, HEIGHT, "untitled Galaxy sim" };
	GLFWwindow* window = initWindow(windowConfig);
	if (!window) {
		return -1;
	}

	setupOpenGL();

    // Initialize Resources
    postProcessor = std::make_unique<PostProcessor>(WIDTH, HEIGHT);

    // Shaders
    try {
        initStars();
        planetShader = std::make_unique<Shader>("assets/shaders/planet.vert", "assets/shaders/planet.frag");
        sunShader = std::make_unique<Shader>("assets/shaders/sun.vert", "assets/shaders/sun.frag");
        blackHoleShader = std::make_unique<Shader>("assets/shaders/blackhole.vert", "assets/shaders/blackhole.frag");
        gasShader = std::make_unique<Shader>("assets/shaders/gas.vert", "assets/shaders/gas.frag");
        orbitShader = std::make_unique<Shader>("assets/shaders/orbit.vert", "assets/shaders/orbit.frag");

        // Uniforms setup
        blackHoleShader->use();
        blackHoleShader->setInt("noiseTexture", 0);

        gasShader->use();

    } catch (const std::exception& e) {
		std::cerr << "Initialization failed: " << e.what() << std::endl;
		return -1;
	}

    // Generate Textures
    std::cout << "Generating Procedural Textures..." << std::endl;
    sunTexture = TextureGenerator::GenerateSunTexture(512, 512, 42);
    planetTexture = TextureGenerator::GeneratePlanetTexture(512, 512, 123, 0.55f, 0.0f, 0.1f, 0.5f, 0.1f, 0.6f, 0.2f); // Earth-like
    noiseTexture = TextureGenerator::GenerateNoiseTexture(256, 256, 5.0f, 0.5f, 4, 999);

	// Generate galaxy
	GalaxyConfig galaxyConfig = createDefaultGalaxyConfig();
	std::vector<Star> stars;
	generateStarField(stars, galaxyConfig);
	uploadStarData(stars);

	BlackHoleConfig blackHoleConfig = createDefaultBlackHoleConfig();
	std::vector<BlackHole> blackHoles;
	generateBlackHoles(blackHoles, blackHoleConfig, galaxyConfig.seed,
		galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

	GasConfig gasConfig = createDefaultGasConfig();
	std::vector<GasVertex> darkGasVertices;
    std::vector<GasVertex> luminousGasVertices;
	generateGalacticGas(darkGasVertices, luminousGasVertices, gasConfig, galaxyConfig.seed,
		galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

	generateSolarSystem();

	std::cout << "Initializing Camera..." << std::endl;
	Camera camera;
	camera.zoomLevel = 0.1;
	camera.zoom = camera.zoomLevel;

	// Position camera to look at the center of the galaxy (0,0,0)
	// Note: Camera position is in scaled view space
	camera.posX = 0.0;
	camera.posY = 1000.0 * camera.zoom;
	camera.posZ = 1500.0 * camera.zoom;
    // Initial rotation: ~33 degrees down (-0.58 rad) around X axis
    camera.orientation = glm::angleAxis(-0.58f, glm::vec3(1.0f, 0.0f, 0.0f));

	MouseState mouseState = { WIDTH / 2.0, HEIGHT / 2.0, true };

	initInput(window, camera, mouseState);

	try {
		std::cout << "Initializing UI..." << std::endl;
		initUI();
		std::cout << "UI Initialized." << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "UI Initialization failed: " << e.what() << std::endl;
	}
	UIState uiState = {};
	uiState.isVisible = false;
	uiState.hoveredButton = -1;
	uiState.activeInput = -1;
	uiState.needsRegeneration = false;
	uiState.tempBlackHoleMass = 4.3f;
	uiState.tempSolarSystemScale = 500.0f;
	uiState.tempTimeSpeed = 1.0f;
	updateUIStateFromConfigs(uiState, galaxyConfig, gasConfig, blackHoleConfig);

	setGlobalUIState(&uiState);

    // Register resize callback
    setResizeCallback([](int w, int h) {
        if (postProcessor) {
            std::cout << "Resizing PostProcessor..." << std::endl;
            postProcessor->Resize(w, h);
        }
    });

	double lastTime = glfwGetTime();
	double fpsTimer = 0.0;
	int frameCount = 0;

	// Main loop
	if (!window) {
		std::cerr << "FATAL: Window is NULL!" << std::endl;
		return -1;
	}

	while (!glfwWindowShouldClose(window)) {
		double currentTime = glfwGetTime();
		double deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		// FPS Calculation
		frameCount++;
		fpsTimer += deltaTime;
		if (fpsTimer >= 1.0) {
			uiState.fps = static_cast<float>(frameCount) / static_cast<float>(fpsTimer);
			frameCount = 0;
			fpsTimer = 0.0;
		}

		double adjustedDeltaTime = deltaTime * g_currentTimeSpeed;

		// Star positions are now updated in the vertex shader
		updateBlackHoles(blackHoles, adjustedDeltaTime);
		// updateGalacticGas removed (Moved to GPU)
		updatePlanets(adjustedDeltaTime);

		handleUIInput(window, uiState, mouseState);

		if (uiState.needsRegeneration) {
			applyUIChangesToConfigs(uiState, galaxyConfig, gasConfig, blackHoleConfig);

			stars.clear();
			generateStarField(stars, galaxyConfig);
			uploadStarData(stars);

			blackHoles.clear();
			generateBlackHoles(blackHoles, blackHoleConfig, galaxyConfig.seed,
				galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

			darkGasVertices.clear();
            luminousGasVertices.clear();
			generateGalacticGas(darkGasVertices, luminousGasVertices, gasConfig, galaxyConfig.seed,
				galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

			std::cout << "Galaxy regenerated with new parameters" << std::endl;
			uiState.needsRegeneration = false;
		}

		processInput(window, camera, &uiState);

		render(stars, blackHoles, darkGasVertices, luminousGasVertices, camera, uiState);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	cleanupStars();
    setResizeCallback(nullptr);
	cleanup(window);
	return 0;
}
