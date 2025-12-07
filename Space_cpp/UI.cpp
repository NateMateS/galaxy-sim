#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "UI.h"
#include "Input.h"
#include "FontRenderer.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float g_currentBlackHoleMass = 4.3f;
float g_currentSolarSystemScale = 500.0f;
float g_currentTimeSpeed = 1.0f;

// UI Render State
static std::unique_ptr<Shader> uiBatchShader;
static unsigned int uiVAO = 0;
static unsigned int uiVBO = 0;
static glm::mat4 uiProjection;

// The Batch Buffer
static std::vector<UIVertex> uiBatchBuffer;

enum ButtonID {
	BTN_NONE = -1,
	BTN_COPY_SEED = 0,
	BTN_STAR_INC,
	BTN_STAR_DEC,
	BTN_STAR_RESET,
	BTN_MOL_INC,
	BTN_MOL_DEC,
	BTN_MOL_RESET,
	BTN_COLD_INC,
	BTN_COLD_DEC,
	BTN_COLD_RESET,
	BTN_WARM_N_INC,
	BTN_WARM_N_DEC,
	BTN_WARM_N_RESET,
	BTN_WARM_I_INC,
	BTN_WARM_I_DEC,
	BTN_WARM_I_RESET,
	BTN_HOT_INC,
	BTN_HOT_DEC,
	BTN_HOT_RESET,
	BTN_CORONAL_INC,
	BTN_CORONAL_DEC,
	BTN_CORONAL_RESET,
	BTN_BH_MASS_INC,
	BTN_BH_MASS_DEC,
	BTN_BH_MASS_RESET,
	BTN_SS_SCALE_INC,
	BTN_SS_SCALE_DEC,
	BTN_SS_SCALE_RESET,
	BTN_TIME_SPEED_INC,
	BTN_TIME_SPEED_DEC,
	BTN_TIME_SPEED_RESET,
	BTN_TOGGLE_TURB,
	BTN_TOGGLE_DENS,
	BTN_TOGGLE_BH,
	BTN_APPLY
};

struct ButtonRect {
	float x, y, width, height;
	ButtonID id;
};

static std::vector<ButtonRect> buttons;
static double mouseX = 0, mouseY = 0;

void initUI() {
	FontRenderer::initFont(1280, 720); // Default size

    uiBatchShader = std::make_unique<Shader>("assets/shaders/ui_batch.vert", "assets/shaders/ui_batch.frag");

    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);

    // Layout matches UIVertex: x, y, u, v, r, g, b, a, mode
    // 9 floats total
    size_t stride = 9 * sizeof(float);

    // 0: Pos (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // 1: UV (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 2: Color (vec4)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // 3: Mode (float)
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void cleanupUI() {
    if (uiVAO) glDeleteVertexArrays(1, &uiVAO);
    if (uiVBO) glDeleteBuffers(1, &uiVBO);
    uiBatchShader.reset();
}

// Draw Call Flush
static void flushUIBatch() {
    if (uiBatchBuffer.empty()) return;

    uiBatchShader->use();
    uiBatchShader->setMat4("projection", glm::value_ptr(uiProjection));
    uiBatchShader->setInt("textTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, FontRenderer::getFontTexture());

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, uiBatchBuffer.size() * sizeof(UIVertex), uiBatchBuffer.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)uiBatchBuffer.size());

    glBindVertexArray(0);
    uiBatchBuffer.clear();
}

static void drawRect(float x, float y, float width, float height,
	float r, float g, float b, float a = 1.0f, bool filled = true) {

    // Mode 1.0 = Rect
    if (filled) {
        // 2 Triangles
        uiBatchBuffer.push_back({x, y, 0, 0, r, g, b, a, 1.0f});
        uiBatchBuffer.push_back({x, y + height, 0, 0, r, g, b, a, 1.0f});
        uiBatchBuffer.push_back({x + width, y + height, 0, 0, r, g, b, a, 1.0f});

        uiBatchBuffer.push_back({x, y, 0, 0, r, g, b, a, 1.0f});
        uiBatchBuffer.push_back({x + width, y + height, 0, 0, r, g, b, a, 1.0f});
        uiBatchBuffer.push_back({x + width, y, 0, 0, r, g, b, a, 1.0f});
    } else {
        // Outline (4 thin filled rects)
        float t = 1.0f; // thickness
        // Top
        drawRect(x, y, width, t, r, g, b, a, true);
        // Bottom
        drawRect(x, y + height - t, width, t, r, g, b, a, true);
        // Left
        drawRect(x, y, t, height, r, g, b, a, true);
        // Right
        drawRect(x + width - t, y, t, height, r, g, b, a, true);
    }
}

static void drawButton(const std::string& label, float x, float y, float width, float height,
	ButtonID id, bool hovered) {

	if (hovered) {
		drawRect(x, y, width, height, 0.35f, 0.4f, 0.45f, 0.95f);
	}
	else {
		drawRect(x, y, width, height, 0.2f, 0.22f, 0.25f, 0.9f);
	}

	drawRect(x, y, width, height, 0.5f, 0.55f, 0.6f, 1.0f, false);

	float textWidth = FontRenderer::getTextWidth(label, 1.0f);
	float textX = x + (width - textWidth) * 0.5f;
	float textY = y + (height * 0.5f) - 8.0f; // approximate center

    FontRenderer::appendText(label, textX, textY, 1.0f, 0.95f, 0.95f, 1.0f, 1.0f, uiBatchBuffer);

	buttons.push_back({ x, y, width, height, id });
}

void toggleUI(UIState& uiState) {
	uiState.isVisible = !uiState.isVisible;
}

void updateUIStateFromConfigs(UIState& uiState, const GalaxyConfig& galaxyConfig,
	const GasConfig& gasConfig, const BlackHoleConfig& blackHoleConfig) {
	uiState.tempStarCount = galaxyConfig.numStars;
	uiState.tempMolecularClouds = gasConfig.numMolecularClouds;
	uiState.tempColdNeutralClouds = gasConfig.numColdNeutralClouds;
	uiState.tempWarmNeutralClouds = gasConfig.numWarmNeutralClouds;
	uiState.tempWarmIonizedClouds = gasConfig.numWarmIonizedClouds;
	uiState.tempHotIonizedClouds = gasConfig.numHotIonizedClouds;
	uiState.tempCoronalClouds = gasConfig.numCoronalClouds;
	uiState.tempEnableTurbulence = gasConfig.enableTurbulence;
	uiState.tempEnableDensityWaves = gasConfig.enableDensityWaves;
	uiState.tempEnableSupermassive = blackHoleConfig.enableSupermassive;
	uiState.tempBlackHoleMass = g_currentBlackHoleMass;
	uiState.tempSolarSystemScale = g_currentSolarSystemScale;
	uiState.tempTimeSpeed = g_currentTimeSpeed;
	uiState.currentSeed = galaxyConfig.seed;
	uiState.needsRegeneration = false;

	// store defaults
	uiState.defaultStarCount = galaxyConfig.numStars;
	uiState.defaultMolecularClouds = gasConfig.numMolecularClouds;
	uiState.defaultColdNeutralClouds = gasConfig.numColdNeutralClouds;
	uiState.defaultWarmNeutralClouds = gasConfig.numWarmNeutralClouds;
	uiState.defaultWarmIonizedClouds = gasConfig.numWarmIonizedClouds;
	uiState.defaultHotIonizedClouds = gasConfig.numHotIonizedClouds;
	uiState.defaultCoronalClouds = gasConfig.numCoronalClouds;
	uiState.defaultEnableTurbulence = gasConfig.enableTurbulence;
	uiState.defaultEnableDensityWaves = gasConfig.enableDensityWaves;
	uiState.defaultEnableSupermassive = blackHoleConfig.enableSupermassive;
	uiState.defaultBlackHoleMass = 4.3f;
	uiState.defaultSolarSystemScale = 500.0f;
	uiState.defaultTimeSpeed = 1.0f;
}

void applyUIChangesToConfigs(const UIState& uiState, GalaxyConfig& galaxyConfig,
	GasConfig& gasConfig, BlackHoleConfig& blackHoleConfig) {
	galaxyConfig.numStars = uiState.tempStarCount;
	gasConfig.numMolecularClouds = uiState.tempMolecularClouds;
	gasConfig.numColdNeutralClouds = uiState.tempColdNeutralClouds;
	gasConfig.numWarmNeutralClouds = uiState.tempWarmNeutralClouds;
	gasConfig.numWarmIonizedClouds = uiState.tempWarmIonizedClouds;
	gasConfig.numHotIonizedClouds = uiState.tempHotIonizedClouds;
	gasConfig.numCoronalClouds = uiState.tempCoronalClouds;
	gasConfig.enableTurbulence = uiState.tempEnableTurbulence;
	gasConfig.enableDensityWaves = uiState.tempEnableDensityWaves;
	blackHoleConfig.enableSupermassive = uiState.tempEnableSupermassive;
	g_currentBlackHoleMass = uiState.tempBlackHoleMass;
	g_currentSolarSystemScale = uiState.tempSolarSystemScale;
	g_currentTimeSpeed = uiState.tempTimeSpeed;
}

static void drawNumberInput(const std::string& label, int value, float x, float y, float width,
	ButtonID incID, ButtonID decID, ButtonID resetID, bool hoveredInc, bool hoveredDec, bool hoveredReset) {
	FontRenderer::appendText(label, x, y, 1.1f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);

	float inputY = y + 22.0f;
	float btnSize = 28.0f;
	float inputWidth = width - btnSize * 3 - 15.0f;

	drawRect(x, inputY, inputWidth, 30.0f, 0.08f, 0.08f, 0.1f, 0.95f);
	drawRect(x, inputY, inputWidth, 30.0f, 0.4f, 0.45f, 0.5f, 0.8f, false);

	std::stringstream ss;
	ss << value;

    FontRenderer::appendText(ss.str(), x + 10, inputY + 7, 1.2f, 1.0f, 1.0f, 1.0f, 1.0f, uiBatchBuffer);

	drawButton("-", x + inputWidth + 5, inputY, btnSize, 30.0f, decID, hoveredDec);
	drawButton("+", x + inputWidth + btnSize + 10, inputY, btnSize, 30.0f, incID, hoveredInc);
	drawButton("R", x + inputWidth + btnSize * 2 + 15, inputY, btnSize, 30.0f, resetID, hoveredReset);
}

static void drawFloatInput(const std::string& label, float value, float x, float y, float width,
	ButtonID incID, ButtonID decID, ButtonID resetID, bool hoveredInc, bool hoveredDec, bool hoveredReset) {

	FontRenderer::appendText(label, x, y, 1.1f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);

	float inputY = y + 22.0f;
	float btnSize = 28.0f;
	float inputWidth = width - btnSize * 3 - 15.0f;

	drawRect(x, inputY, inputWidth, 30.0f, 0.08f, 0.08f, 0.1f, 0.95f);
	drawRect(x, inputY, inputWidth, 30.0f, 0.4f, 0.45f, 0.5f, 0.8f, false);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(1) << value;

    FontRenderer::appendText(ss.str(), x + 10, inputY + 7, 1.2f, 1.0f, 1.0f, 1.0f, 1.0f, uiBatchBuffer);

	drawButton("-", x + inputWidth + 5, inputY, btnSize, 30.0f, decID, hoveredDec);
	drawButton("+", x + inputWidth + btnSize + 10, inputY, btnSize, 30.0f, incID, hoveredInc);
	drawButton("R", x + inputWidth + btnSize * 2 + 15, inputY, btnSize, 30.0f, resetID, hoveredReset);
}

static void drawToggle(const std::string& label, bool value, float x, float y,
	ButtonID toggleID, bool hovered) {
	float boxSize = 24.0f;

	if (hovered) {
		drawRect(x, y, boxSize, boxSize, 0.3f, 0.32f, 0.35f, 0.95f);
	}
	else {
		drawRect(x, y, boxSize, boxSize, 0.2f, 0.22f, 0.25f, 0.95f);
	}
	drawRect(x, y, boxSize, boxSize, 0.5f, 0.55f, 0.6f, 1.0f, false);

	if (value) {
		drawRect(x + 6, y + 6, boxSize - 12, boxSize - 12, 0.3f, 0.8f, 0.5f, 1.0f);
	}

    FontRenderer::appendText(label, x + boxSize + 12, y + 3, 1.1f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);

	buttons.push_back({ x, y, boxSize, boxSize, toggleID });
}

void renderUI(UIState& uiState, int screenWidth, int screenHeight) {
	if (!uiState.isVisible) return;

    // Ensure font init
    FontRenderer::initFont(screenWidth, screenHeight);

    // Update global projection
    uiProjection = glm::ortho(0.0f, (float)screenWidth, (float)screenHeight, 0.0f);

	buttons.clear();
    // Ensure buffer is cleared at start of frame
    uiBatchBuffer.clear();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float padding = 20.0f;
	float panelWidth = 450.0f;
	float panelX = padding;
	float panelY = padding;
	float contentWidth = panelWidth - padding * 2;

	float panelHeight = screenHeight - padding * 2;

	drawRect(panelX, panelY, panelWidth, panelHeight, 0.08f, 0.08f, 0.12f, 0.92f);
	drawRect(panelX, panelY, panelWidth, panelHeight, 0.4f, 0.45f, 0.5f, 0.9f, false);

	float currentY = panelY + padding;
	float itemX = panelX + padding;

    FontRenderer::appendText("SIMULATION PARAMETERS", itemX, currentY, 1.4f, 0.4f, 0.8f, 1.0f, 1.0f, uiBatchBuffer);
	currentY += 35.0f;

    FontRenderer::appendText("Galaxy Seed:", itemX, currentY, 1.1f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 25.0f;

	std::stringstream seedStr;
	seedStr << uiState.currentSeed;

	float seedBoxWidth = contentWidth - 85.0f;
	drawRect(itemX, currentY, seedBoxWidth, 32.0f, 0.08f, 0.08f, 0.1f, 0.95f);
	drawRect(itemX, currentY, seedBoxWidth, 32.0f, 0.4f, 0.45f, 0.5f, 0.8f, false);
    FontRenderer::appendText(seedStr.str(), itemX + 10, currentY + 8, 1.2f, 1.0f, 1.0f, 1.0f, 1.0f, uiBatchBuffer);

	bool hoveredCopy = (mouseX >= itemX + seedBoxWidth + 10 && mouseX <= itemX + contentWidth &&
		mouseY >= currentY && mouseY <= currentY + 32);
	drawButton("Copy", itemX + seedBoxWidth + 10, currentY, 70.0f, 32.0f, BTN_COPY_SEED, hoveredCopy);
	currentY += 50.0f;

	auto isHovered = [](ButtonID id) {
		for (const auto& btn : buttons) {
			if (btn.id == id && mouseX >= btn.x && mouseX <= btn.x + btn.width &&
				mouseY >= btn.y && mouseY <= btn.y + btn.height) {
				return true;
			}
		}
		return false;
		};

	drawNumberInput("Star Count", uiState.tempStarCount, itemX, currentY, contentWidth,
		BTN_STAR_INC, BTN_STAR_DEC, BTN_STAR_RESET,
		isHovered(BTN_STAR_INC), isHovered(BTN_STAR_DEC), isHovered(BTN_STAR_RESET));
	currentY += 70.0f;

    FontRenderer::appendText("Simulation:", itemX, currentY, 1.2f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 30.0f;

	drawFloatInput("Time Speed", uiState.tempTimeSpeed, itemX + 15, currentY, contentWidth - 15,
		BTN_TIME_SPEED_INC, BTN_TIME_SPEED_DEC, BTN_TIME_SPEED_RESET,
		isHovered(BTN_TIME_SPEED_INC), isHovered(BTN_TIME_SPEED_DEC), isHovered(BTN_TIME_SPEED_RESET));
	currentY += 70.0f;

    FontRenderer::appendText("Black Hole:", itemX, currentY, 1.2f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 30.0f;

	drawFloatInput("Mass (Million M?)", uiState.tempBlackHoleMass, itemX + 15, currentY, contentWidth - 15,
		BTN_BH_MASS_INC, BTN_BH_MASS_DEC, BTN_BH_MASS_RESET,
		isHovered(BTN_BH_MASS_INC), isHovered(BTN_BH_MASS_DEC), isHovered(BTN_BH_MASS_RESET));
	currentY += 70.0f;

    FontRenderer::appendText("Solar System:", itemX, currentY, 1.2f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 30.0f;

	drawFloatInput("Scale Multiplier", uiState.tempSolarSystemScale, itemX + 15, currentY, contentWidth - 15,
		BTN_SS_SCALE_INC, BTN_SS_SCALE_DEC, BTN_SS_SCALE_RESET,
		isHovered(BTN_SS_SCALE_INC), isHovered(BTN_SS_SCALE_DEC), isHovered(BTN_SS_SCALE_RESET));
	currentY += 70.0f;

    FontRenderer::appendText("Gas Clouds:", itemX, currentY, 1.2f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 30.0f;

	drawNumberInput("Molecular", uiState.tempMolecularClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_MOL_INC, BTN_MOL_DEC, BTN_MOL_RESET,
		isHovered(BTN_MOL_INC), isHovered(BTN_MOL_DEC), isHovered(BTN_MOL_RESET));
	currentY += 65.0f;

	drawNumberInput("Cold Neutral", uiState.tempColdNeutralClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_COLD_INC, BTN_COLD_DEC, BTN_COLD_RESET,
		isHovered(BTN_COLD_INC), isHovered(BTN_COLD_DEC), isHovered(BTN_COLD_RESET));
	currentY += 65.0f;

	drawNumberInput("Warm Neutral", uiState.tempWarmNeutralClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_WARM_N_INC, BTN_WARM_N_DEC, BTN_WARM_N_RESET,
		isHovered(BTN_WARM_N_INC), isHovered(BTN_WARM_N_DEC), isHovered(BTN_WARM_N_RESET));
	currentY += 65.0f;

	drawNumberInput("Warm Ionized", uiState.tempWarmIonizedClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_WARM_I_INC, BTN_WARM_I_DEC, BTN_WARM_I_RESET,
		isHovered(BTN_WARM_I_INC), isHovered(BTN_WARM_I_DEC), isHovered(BTN_WARM_I_RESET));
	currentY += 65.0f;

	drawNumberInput("Hot Ionized", uiState.tempHotIonizedClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_HOT_INC, BTN_HOT_DEC, BTN_HOT_RESET,
		isHovered(BTN_HOT_INC), isHovered(BTN_HOT_DEC), isHovered(BTN_HOT_RESET));
	currentY += 65.0f;

	drawNumberInput("Coronal", uiState.tempCoronalClouds, itemX + 15, currentY, contentWidth - 15,
		BTN_CORONAL_INC, BTN_CORONAL_DEC, BTN_CORONAL_RESET,
		isHovered(BTN_CORONAL_INC), isHovered(BTN_CORONAL_DEC), isHovered(BTN_CORONAL_RESET));
	currentY += 75.0f;

    FontRenderer::appendText("Options:", itemX, currentY, 1.2f, 0.85f, 0.85f, 0.95f, 1.0f, uiBatchBuffer);
	currentY += 30.0f;

	drawToggle("Enable Turbulence", uiState.tempEnableTurbulence, itemX + 15, currentY,
		BTN_TOGGLE_TURB, isHovered(BTN_TOGGLE_TURB));
	currentY += 35.0f;

	drawToggle("Enable Density Waves", uiState.tempEnableDensityWaves, itemX + 15, currentY,
		BTN_TOGGLE_DENS, isHovered(BTN_TOGGLE_DENS));
	currentY += 35.0f;

	drawToggle("Supermassive Black Hole", uiState.tempEnableSupermassive, itemX + 15, currentY,
		BTN_TOGGLE_BH, isHovered(BTN_TOGGLE_BH));
	currentY += 50.0f;

	bool hoveredApply = isHovered(BTN_APPLY);
	drawButton("Apply Changes", itemX, currentY, contentWidth, 40.0f, BTN_APPLY, hoveredApply);
	currentY += 50.0f;

    FontRenderer::appendText("Press TAB to close | ESC to exit", itemX, currentY, 0.95f, 0.6f, 0.6f, 0.7f, 1.0f, uiBatchBuffer);

	// FPS Counter (Top Right)
	std::stringstream fpsStream;
	fpsStream << "FPS: " << static_cast<int>(uiState.fps);
	std::string fpsStr = fpsStream.str();
	float fpsWidth = FontRenderer::getTextWidth(fpsStr, 1.2f);
    FontRenderer::appendText(fpsStr, screenWidth - fpsWidth - 20.0f, 20.0f, 1.2f, 0.0f, 1.0f, 0.0f, 1.0f, uiBatchBuffer);

    // FLUSH THE BATCH
    flushUIBatch();

	glEnable(GL_DEPTH_TEST);
}

void handleUIInput(GLFWwindow* window, UIState& uiState, MouseState& mouseState) {
	glfwGetCursorPos(window, &mouseX, &mouseY);

	static bool tabWasPressed = false;
	bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;

	if (tabPressed && !tabWasPressed) {
		toggleUI(uiState);

		if (uiState.isVisible) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			mouseState.firstMouse = true;
		}
	}
	tabWasPressed = tabPressed;

	if (!uiState.isVisible) return;

	static bool mouseWasPressed = false;
	bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

	if (mousePressed && !mouseWasPressed) {
		for (const auto& btn : buttons) {
			if (mouseX >= btn.x && mouseX <= btn.x + btn.width &&
				mouseY >= btn.y && mouseY <= btn.y + btn.height) {

				switch (btn.id) {
				case BTN_COPY_SEED: {
					std::stringstream ss;
					ss << uiState.currentSeed;
					std::string seedStr = ss.str();

#ifdef _WIN32
					if (OpenClipboard(nullptr)) {
						EmptyClipboard();
						HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, seedStr.size() + 1);
						if (hg) {
							memcpy(GlobalLock(hg), seedStr.c_str(), seedStr.size() + 1);
							GlobalUnlock(hg);
							SetClipboardData(CF_TEXT, hg);
						}
						CloseClipboard();
						std::cout << "Seed copied to clipboard: " << seedStr << std::endl;
					}
#else
					std::cout << "Seed (copy manually): " << seedStr << std::endl;
#endif
					break;
				}

				case BTN_STAR_INC: uiState.tempStarCount = std::min(4000000, uiState.tempStarCount + 100000); break;
				case BTN_STAR_DEC: uiState.tempStarCount = std::max(1000, uiState.tempStarCount - 100000); break;
				case BTN_STAR_RESET: uiState.tempStarCount = uiState.defaultStarCount; break;

				case BTN_TIME_SPEED_INC: uiState.tempTimeSpeed = std::min(100.0f, uiState.tempTimeSpeed + 0.5f); break;
				case BTN_TIME_SPEED_DEC: uiState.tempTimeSpeed = std::max(0.0f, uiState.tempTimeSpeed - 0.5f); break;
				case BTN_TIME_SPEED_RESET: uiState.tempTimeSpeed = uiState.defaultTimeSpeed; break;

				case BTN_BH_MASS_INC: uiState.tempBlackHoleMass = uiState.tempBlackHoleMass + 0.5f; break;
				case BTN_BH_MASS_DEC: uiState.tempBlackHoleMass = std::max(0.1f, uiState.tempBlackHoleMass - 0.5f); break;
				case BTN_BH_MASS_RESET: uiState.tempBlackHoleMass = uiState.defaultBlackHoleMass; break;

				case BTN_SS_SCALE_INC: uiState.tempSolarSystemScale = uiState.tempSolarSystemScale + 50.0f; break;
				case BTN_SS_SCALE_DEC: uiState.tempSolarSystemScale = std::max(100.0f, uiState.tempSolarSystemScale - 50.0f); break;
				case BTN_SS_SCALE_RESET: uiState.tempSolarSystemScale = uiState.defaultSolarSystemScale; break;

				case BTN_MOL_INC: uiState.tempMolecularClouds = std::min(20000, uiState.tempMolecularClouds + 500); break;
				case BTN_MOL_DEC: uiState.tempMolecularClouds = std::max(0, uiState.tempMolecularClouds - 500); break;
				case BTN_MOL_RESET: uiState.tempMolecularClouds = uiState.defaultMolecularClouds; break;

				case BTN_COLD_INC: uiState.tempColdNeutralClouds = std::min(40000, uiState.tempColdNeutralClouds + 1000); break;
				case BTN_COLD_DEC: uiState.tempColdNeutralClouds = std::max(0, uiState.tempColdNeutralClouds - 1000); break;
				case BTN_COLD_RESET: uiState.tempColdNeutralClouds = uiState.defaultColdNeutralClouds; break;

				case BTN_WARM_N_INC: uiState.tempWarmNeutralClouds = std::min(40000, uiState.tempWarmNeutralClouds + 1000); break;
				case BTN_WARM_N_DEC: uiState.tempWarmNeutralClouds = std::max(0, uiState.tempWarmNeutralClouds - 1000); break;
				case BTN_WARM_N_RESET: uiState.tempWarmNeutralClouds = uiState.defaultWarmNeutralClouds; break;

				case BTN_WARM_I_INC: uiState.tempWarmIonizedClouds = std::min(10000, uiState.tempWarmIonizedClouds + 200); break;
				case BTN_WARM_I_DEC: uiState.tempWarmIonizedClouds = std::max(0, uiState.tempWarmIonizedClouds - 200); break;
				case BTN_WARM_I_RESET: uiState.tempWarmIonizedClouds = uiState.defaultWarmIonizedClouds; break;

				case BTN_HOT_INC: uiState.tempHotIonizedClouds = std::min(10000, uiState.tempHotIonizedClouds + 200); break;
				case BTN_HOT_DEC: uiState.tempHotIonizedClouds = std::max(0, uiState.tempHotIonizedClouds - 200); break;
				case BTN_HOT_RESET: uiState.tempHotIonizedClouds = uiState.defaultHotIonizedClouds; break;

				case BTN_CORONAL_INC: uiState.tempCoronalClouds = std::min(20000, uiState.tempCoronalClouds + 500); break;
				case BTN_CORONAL_DEC: uiState.tempCoronalClouds = std::max(0, uiState.tempCoronalClouds - 500); break;
				case BTN_CORONAL_RESET: uiState.tempCoronalClouds = uiState.defaultCoronalClouds; break;

				case BTN_TOGGLE_TURB: uiState.tempEnableTurbulence = !uiState.tempEnableTurbulence; break;
				case BTN_TOGGLE_DENS: uiState.tempEnableDensityWaves = !uiState.tempEnableDensityWaves; break;
				case BTN_TOGGLE_BH: uiState.tempEnableSupermassive = !uiState.tempEnableSupermassive; break;

				case BTN_APPLY:
					uiState.needsRegeneration = true;
					std::cout << "Applying changes and regenerating galaxy..." << std::endl;
					break;
				}

				break;
			}
		}
	}

	mouseWasPressed = mousePressed;
}
