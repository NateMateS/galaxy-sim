#pragma once
#include <functional>

struct WindowConfig {
 int width;
    int height;
    const char* title;
};

extern int WIDTH;
extern int HEIGHT;

using ResizeCallback = std::function<void(int, int)>;
void setResizeCallback(ResizeCallback callback);

struct GLFWwindow* initWindow(const WindowConfig& config);

void setupOpenGL();
void cleanup(struct GLFWwindow* window);

void framebufferSizeCallback(struct GLFWwindow* window, int width, int height);
