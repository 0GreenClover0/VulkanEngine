#include <string>
#include <stdexcept>

#include "Window.hpp"

#include "Renderer.hpp"

Window::Window(int width, int height, std::string name)
{
	glfw_window = init_window(width, height, name);
}

GLFWwindow* Window::get_glfw_window() const
{
	return glfw_window;
}

GLFWwindow* Window::init_window(int width, int height, std::string name)
{
	GLFWwindow* window = nullptr;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);

	if (window == nullptr) {
		throw std::runtime_error("Could not create a window.");
	}

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);

	return window;
}

void Window::framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->set_framebuffer_as_resized();
}