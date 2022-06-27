#pragma once

#include <GLFW/glfw3.h>

class Window
{
public:
	Window(int width, int height, std::string name);
	~Window() = default;
	GLFWwindow* get_glfw_window() const;

private:
	GLFWwindow* glfw_window;

	GLFWwindow* init_window(int width, int height, std::string name);
	static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
};

