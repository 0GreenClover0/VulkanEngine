#include "ModelLoader.hpp"
#include "Renderer.hpp"
#include "Window.hpp"

int main()
{
	// TODO: move this to some config class/file?
	const uint32_t WIDTH = 1920;
	const uint32_t HEIGHT = 1080;
	const std::string WINDOW_NAME = "Game engine";

	Renderer renderer;

	try
	{
		// Window initialization
		Window window(WIDTH, HEIGHT, WINDOW_NAME);
		GLFWwindow* glfw_window = window.get_glfw_window();

		renderer.set_glfw_window(glfw_window);

		// Vulkan initalization
		renderer.init_vulkan();
		
		// Main loop
		while (!glfwWindowShouldClose(glfw_window))
		{
			glfwPollEvents();
			renderer.draw_frame();
		}

		// Cleanup
		vkDeviceWaitIdle(renderer.get_device());
		renderer.cleanup();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}