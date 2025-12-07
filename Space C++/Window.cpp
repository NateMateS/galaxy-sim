#include "Window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

static ResizeCallback g_ResizeCallback = nullptr;

void setResizeCallback(ResizeCallback callback) {
    g_ResizeCallback = callback;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	WIDTH = width;
	HEIGHT = height;

	glViewport(0, 0, width, height);
    if (g_ResizeCallback) {
        g_ResizeCallback(width, height);
    }
}

GLFWwindow* initWindow(const WindowConfig& config) {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return nullptr;
	}

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return nullptr;
	}

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glViewport(0, 0, config.width, config.height);

	return window;
}

void setupOpenGL() {
	glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE); // Ensure shaders can control point size
    // GL_POINT_SMOOTH is deprecated in Core Profile.
    // Modern GL uses Program Point Size and fragment shaders for smooth circles (done in shaders).

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
}

void cleanup(GLFWwindow* window) {
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}
