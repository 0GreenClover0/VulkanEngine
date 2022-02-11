#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

class App {

public:

	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

#ifdef NDEBUG
	const bool enable_validation_layers = false;
#else
	const bool enable_validation_layers = true;
#endif // NDEBUG

	void run()
	{
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:

	GLFWwindow* window;
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkQueue graphics_queue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swap_chain;

	const std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	struct QueueFamilyIndices
	{
		uint32_t graphics_family;
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	void init_window()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void init_vulkan()
	{
		create_instance();
		//setup_debug_messenger();
		// Window surface needs to be created right after the instance creation, because it can actually influence the physical device selection
		create_surface();
		pick_physical_device();
		create_logical_device();
		create_swap_chain();
	}

	void main_loop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void create_instance()
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Vulkan Engine";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "Vulkan Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions;

		glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		create_info.enabledExtensionCount = glfw_extension_count;
		create_info.ppEnabledExtensionNames = glfw_extensions;
		create_info.enabledLayerCount = 0;

		uint32_t extension_count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

		std::vector<VkExtensionProperties> extensions(extension_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

		std::cout << "Available extensions:\n";

		for (const auto& extension : extensions)
		{
			std::cout << extension.extensionName << "\n";
		}

		std::cout << "\n";

		for (uint32_t i = 0; i < glfw_extension_count; i++)
		{
			uint32_t j = 0;
			for (j = 0; j < extension_count; j++)
			{
				if (std::strcmp(extensions[j].extensionName, glfw_extensions[i]) == 0)
					break;
			}

			if (j == extension_count)
			{
				std::cout << "Required extension: " << glfw_extensions[i] << " not supported. Aborting...";
				return;
			}
		}

		if (enable_validation_layers && !check_validation_layer_support())
			throw std::runtime_error("Validation layers requested, but not supported.");
		else if (enable_validation_layers)
		{
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();
		}
		else
		{
			create_info.enabledLayerCount = 0;
		}

		if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS)
			throw std::runtime_error("Failed to create an instance.");
	}

	bool check_validation_layer_support()
	{
		uint32_t layer_count = 0;

		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

		std::vector<VkLayerProperties> available_layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		for (const char* layer_name : validation_layers)
		{
			bool layer_found = false;

			for (const auto& layer_properties : available_layers)
			{
				if (std::strcmp(layer_name, layer_properties.layerName) == 0)
				{
					layer_found = true;
					break;
				}
			}

			if (!layer_found)
			{
				return false;
			}
		}

		return true;
	}

	void create_surface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
			throw std::runtime_error("Failed to create window surface.");
	}

	void pick_physical_device()
	{
		uint32_t device_count = 0;

		vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

		if (device_count == 0)
			throw std::runtime_error("Failed to fing GPUs with Vulkan support.");

		std::vector<VkPhysicalDevice> devices(device_count);
		vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

		for (const auto& device : devices)
		{
			if (is_device_suitable(device))
			{
				physical_device = device;
				break;
			}
		}

		if (physical_device == VK_NULL_HANDLE)
			throw std::runtime_error("Failed to find a suitable GPU.");
	}

	bool is_device_suitable(VkPhysicalDevice device)
	{
		// Check for properties or features (like multipass)
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(device, &device_properties);

		VkPhysicalDeviceFeatures device_features;
		vkGetPhysicalDeviceFeatures(device, &device_features);

		QueueFamilyIndices indices;
		if (!find_queue_indices(device, indices))
			return false;

		if (!device_features.geometryShader)
			return false;

		if (!check_device_extensions(device))
			return false;

		SwapChainSupportDetails swap_chain_support_details = query_swap_chain_support(device);
		if (swap_chain_support_details.formats.empty() || swap_chain_support_details.present_modes.empty())
			return false;

		return true;
	}

	bool find_queue_indices(VkPhysicalDevice device, QueueFamilyIndices& indices)
	{
		QueueFamilyIndices new_indices{};

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

		std::vector<VkQueueFamilyProperties> queue_families_properties(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families_properties.data());

		uint32_t i = 0;
		bool appropriate_family_found = false;
		for (const auto& queue_family_properties : queue_families_properties)
		{
			// We want a device that can draw AND display it to the surface
			if (queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				VkBool32 surface_supported = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &surface_supported);

				if (!surface_supported)
					continue;

				new_indices.graphics_family = i;
				appropriate_family_found = true;
				break;
			}

			i++;
		}

		indices = new_indices;
		return appropriate_family_found;
	}

	bool check_device_extensions(VkPhysicalDevice device)
	{
		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

		std::vector<VkExtensionProperties> available_extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

		std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

		for (const auto& extension : available_extensions)
		{
			required_extensions.erase(extension.extensionName);
		}

		return required_extensions.empty();
	}

	void create_logical_device()
	{
		QueueFamilyIndices indices{};
		find_queue_indices(physical_device, indices);

		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = indices.graphics_family;
		queue_create_info.queueCount = 1;

		float queue_priority = 1.0;
		queue_create_info.pQueuePriorities = &queue_priority;

		VkPhysicalDeviceFeatures device_features{};

		VkDeviceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = &queue_create_info;
		create_info.queueCreateInfoCount = 1;
		create_info.pEnabledFeatures = &device_features;

		// Enable extensions (like VK_KHR_swapchain)
		create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
		create_info.ppEnabledExtensionNames = device_extensions.data();

		if (enable_validation_layers)
		{
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();
		}
		else
		{
			create_info.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS)
			throw std::runtime_error("Failed to create a logical device.");

		vkGetDeviceQueue(device, indices.graphics_family, 0, &graphics_queue);
	}

	// Just checking if a swap chain is available is not sufficient, because it may not actually be compatible with our window surface
	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t format_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

		if (format_count != 0)
		{
			details.formats.resize(format_count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
		}

		uint32_t present_mode_count = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

		if (present_mode_count != 0)
		{
			details.present_modes.resize(present_mode_count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
	{
		for (const auto& available_format : available_formats)
		{
			// We prefer format that stores the B, G, R and alpha channels in that order with an 8 bit unsigned integer for a total of 32 bits per pixel.
			if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return available_format;
		}

		// If nothing was found just return the first available format. (We could also rank them but... it's not really that important)
		return available_formats[0];
	}

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
	{
		for (const auto& available_mode : available_present_modes)
		{
			// We prefer triple buffering
			if (available_mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return available_mode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		// Some weird things with resolution

		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actual_extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actual_extent;
	}

	// https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain
	void create_swap_chain()
	{
		SwapChainSupportDetails swap_chain_support_details = query_swap_chain_support(physical_device);

		VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support_details.formats);
		VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support_details.present_modes);
		VkExtent2D extent = choose_swap_extent(swap_chain_support_details.capabilities);

		// Aside from these properties we also have to decide how many images we would like to have in the swap chain.
		// The implementation specifies the minimum number that it requires to function.
		// However, simply sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations
		// before we can acquire another image to render to. Therefore it is recommended to request at least one more image than the minimum.
		uint32_t image_count = swap_chain_support_details.capabilities.minImageCount + 1;

		// We should also make sure to not exceed the maximum number of images while doing this, where 0 is a special value that means that there is no maximum
		if (swap_chain_support_details.capabilities.maxImageCount > 0 && image_count > swap_chain_support_details.capabilities.maxImageCount)
			image_count = swap_chain_support_details.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = surface;
		create_info.minImageCount = image_count;
		create_info.imageFormat = surface_format.format;
		create_info.imageColorSpace = surface_format.colorSpace;
		create_info.imageExtent = extent;
		// The imageArrayLayers specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application.
		create_info.imageArrayLayers = 1;
		// The imageUsage bit field specifies what kind of operations we'll use the images in the swap chain for.
		// Here, we are going to render directly to them, which means that they're used as color attachment. It is also possible to render images
		// to a separate image first to perform operations like post-processing. In that case we use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead
		// and use a memory operation to transfer the rendered image to a swap chain image.
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
		create_info.preTransform = swap_chain_support_details.capabilities.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.presentMode = present_mode;
		create_info.clipped = VK_TRUE;
		create_info.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain) != VK_SUCCESS)
			throw std::runtime_error("Failed to create swap chain.");
	}

	void cleanup()
	{
		vkDestroySwapchainKHR(device, swap_chain, nullptr);

		vkDestroyDevice(device, nullptr);

		// Surface destroyed before the instance
		vkDestroySurfaceKHR(instance, surface, nullptr);

		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}
};

int main()
{
	App app;

	try
	{
		app.run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}