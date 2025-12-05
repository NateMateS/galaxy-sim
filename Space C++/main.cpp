#include <GLFW/glfw3.h>
#include <iostream>
#include <random>
#include <ctime>
#include "Window.h"
#include "Camera.h"
#include "Stars.h"
#include "SolarSystem.h"
#include "BlackHole.h"
#include "GalacticGas.h"
#include "Input.h"
#include "UI.h"

int WIDTH = 1920;
int HEIGHT = 1080;

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
	const std::vector<GasCloud>& gasClouds, const Camera& camera, UIState& uiState) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	setupCamera(camera, WIDTH, HEIGHT, solarSystem);

	RenderZone zone = calculateRenderZone(camera);

	renderStars(stars, zone);

	renderGalacticGas(gasClouds, zone);
	renderBlackHoles(blackHoles, zone);

	if (solarSystem.isGenerated) {
		renderSolarSystem(zone);
	}

	renderUI(uiState, WIDTH, HEIGHT);
}

int main() {
	srand(static_cast<unsigned int>(time(nullptr)));

	WindowConfig windowConfig = { WIDTH, HEIGHT, "untitled Galaxy sim" };
	GLFWwindow* window = initWindow(windowConfig);
	if (!window) {
		return -1;
	}

	setupOpenGL();
	try {
		initStars();
	} catch (const std::exception& e) {
		std::cerr << "Initialization failed: " << e.what() << std::endl;
		return -1;
	}

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
	std::vector<GasCloud> gasClouds;
	generateGalacticGas(gasClouds, gasConfig, galaxyConfig.seed,
		galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

	generateSolarSystem();

	Camera camera;
	camera.zoomLevel = 0.1;
	camera.zoom = camera.zoomLevel;

	// Position camera to look at the center of the galaxy (0,0,0)
	// Note: Camera position is in scaled view space
	camera.posX = 0.0;
	camera.posY = 1000.0 * camera.zoom;
	camera.posZ = 1500.0 * camera.zoom;
	camera.pitch = -0.58; // ~33 degrees down
	camera.yaw = 0.0;

	MouseState mouseState = { WIDTH / 2.0, HEIGHT / 2.0, true };

	initInput(window, camera, mouseState);

	initUI();
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

	double lastTime = glfwGetTime();
	double fpsTimer = 0.0;
	int frameCount = 0;

	// Main loop
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
		updateGalacticGas(gasClouds, adjustedDeltaTime);
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

			gasClouds.clear();
			generateGalacticGas(gasClouds, gasConfig, galaxyConfig.seed,
				galaxyConfig.diskRadius, galaxyConfig.bulgeRadius);

			std::cout << "Galaxy regenerated with new parameters" << std::endl;
			uiState.needsRegeneration = false;
		}

		processInput(window, camera, &uiState);
		render(stars, blackHoles, gasClouds, camera, uiState);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	cleanupStars();
	cleanup(window);
	return 0;
}
