#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

class App {

public:

	bool was_window_resized() { return framebuffer_resized; }

	const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
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
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkCommandPool command_pool;

	std::vector<VkSemaphore> image_available_semaphores;
	std::vector<VkSemaphore> render_finished_semaphores;
	std::vector<VkImage> swap_chain_images;
	std::vector<VkImageView> swap_chain_image_views;
	std::vector<VkFramebuffer> swap_chain_framebuffers;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;

	size_t current_frame = 0;
	bool framebuffer_resized = false;

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

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
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
		create_image_views();
		create_render_pass();
		create_graphics_pipeline();
		create_framebuffers();
		create_command_pool();
		create_command_buffers();
		create_sync_objects();
	}

	void recreate_swap_chain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		// Wait until old swap chain is not being used
		vkDeviceWaitIdle(device);

		cleanup_swap_chain();

		create_swap_chain();
		create_image_views();
		create_render_pass();
		create_graphics_pipeline();
		create_framebuffers();

		if (swap_chain_images.size() != command_buffers.size())
		{
			vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
			create_command_buffers();
		}
	}

	void main_loop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			draw_frame();
		}

		vkDeviceWaitIdle(device);
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

		vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
		swap_chain_images.resize(image_count);
		vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());

		swap_chain_image_format = surface_format.format;
		swap_chain_extent = extent;
	}

	void create_image_views()
	{
		swap_chain_image_views.resize(swap_chain_images.size());

		for (int i = 0; i < swap_chain_images.size(); i++)
		{
			VkImageViewCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = swap_chain_images[i];
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			create_info.format = swap_chain_image_format;
			create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &create_info, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS)
				throw std::runtime_error("Failed to create an image view");
		}
	}

	void create_render_pass()
	{
		VkAttachmentDescription color_attachment{};
		color_attachment.format = swap_chain_image_format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment_ref{};
		color_attachment_ref.attachment = 0; // We only have one attachment description so its index is 0
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1; // The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive
		subpass.pColorAttachments = &color_attachment_ref;

		VkRenderPassCreateInfo render_pass_create_info{};
		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.attachmentCount = 1;
		render_pass_create_info.pAttachments = &color_attachment;
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = &subpass;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Subpass-dependencies
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
			throw std::runtime_error("Failed to create render pass!");
	}

	void create_graphics_pipeline()
	{
		auto vert_shader_code = read_file("shaders/vert.spv");
		auto frag_shader_code = read_file("shaders/frag.spv");

		// The compilation and linking of the SPIR-V bytecode to machine code for execution by the GPU doesn't happen until the graphics pipeline is created.
		// That means that we're allowed to destroy the shader modules again as soon as pipeline creation is finished.
		VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
		VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

		VkPipelineShaderStageCreateInfo vert_shader_stage_create_info{};
		vert_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vert_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vert_shader_stage_create_info.module = vert_shader_module;
		vert_shader_stage_create_info.pName = "main";

		VkPipelineShaderStageCreateInfo frag_shader_stage_create_info{};
		frag_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		frag_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		frag_shader_stage_create_info.module = frag_shader_module;
		frag_shader_stage_create_info.pName = "main";

		VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_create_info, frag_shader_stage_create_info};

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Vertex-input
		VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
		vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_create_info.vertexBindingDescriptionCount = 0;
		vertex_input_create_info.pVertexBindingDescriptions = nullptr;
		vertex_input_create_info.vertexAttributeDescriptionCount = 0;
		vertex_input_create_info.pVertexAttributeDescriptions = nullptr;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Input-assembly
		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
		input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Viewports-and-scissors
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) swap_chain_extent.width;
		viewport.height = (float) swap_chain_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swap_chain_extent;

		VkPipelineViewportStateCreateInfo viewport_state_create_info{};
		viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_create_info.viewportCount = 1;
		viewport_state_create_info.pViewports = &viewport;
		viewport_state_create_info.scissorCount = 1;
		viewport_state_create_info.pScissors = &scissor;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer_create_info{};
		rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer_create_info.depthClampEnable = VK_FALSE;
		rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer_create_info.lineWidth = 1.0f;
		rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer_create_info.depthBiasEnable = VK_FALSE;
		rasterizer_create_info.depthBiasConstantFactor = 0.0f;
		rasterizer_create_info.depthBiasClamp = 0.0f;
		rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Multisampling
		VkPipelineMultisampleStateCreateInfo multisampling_create_info{};
		multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling_create_info.sampleShadingEnable = VK_FALSE;
		multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling_create_info.minSampleShading = 1.0f;
		multisampling_create_info.pSampleMask = nullptr;
		multisampling_create_info.alphaToCoverageEnable = VK_FALSE;
		multisampling_create_info.alphaToOneEnable = VK_FALSE;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Depth-and-stencil-testing
		// Nothing

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Color-blending
		VkPipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = VK_TRUE;
		// Alpha blending
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		// We are not using this
		VkPipelineColorBlendStateCreateInfo color_blending{};
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;
		color_blending.blendConstants[0] = 0.0f; // Optional
		color_blending.blendConstants[1] = 0.0f; // Optional
		color_blending.blendConstants[2] = 0.0f; // Optional
		color_blending.blendConstants[3] = 0.0f; // Optional

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Pipeline-layout
		VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 0;
		pipeline_layout_create_info.pSetLayouts = nullptr;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create pipeline layout.");

		VkGraphicsPipelineCreateInfo pipeline_create_info{};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stages;
		pipeline_create_info.pVertexInputState = &vertex_input_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_create_info.pViewportState = &viewport_state_create_info;
		pipeline_create_info.pRasterizationState = &rasterizer_create_info;
		pipeline_create_info.pMultisampleState = &multisampling_create_info;
		pipeline_create_info.pDepthStencilState = nullptr;
		pipeline_create_info.pColorBlendState = &color_blending;
		pipeline_create_info.pDynamicState = nullptr;
		pipeline_create_info.layout = pipeline_layout;
		pipeline_create_info.renderPass = render_pass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_create_info.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &graphics_pipeline) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipeline!");

		vkDestroyShaderModule(device, vert_shader_module, nullptr);
		vkDestroyShaderModule(device, frag_shader_module, nullptr);
	}

	VkShaderModule create_shader_module(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = code.size();

		// Size of the bytecode is specified in bytes, but the bytecode pointer is a uint32_t pointer rather than a char pointer.
		// Therefore we will need to cast the pointer with reinterpret_cast as shown below. When performing a cast like this, we also need to ensure that
		// the data satisfies the alignment requirements of uint32_t. Lucky for us, the data is stored in an std::vector where
		// the default allocator already ensures that the data satisfies the worst case alignment requirements.
		// NOTE: what does it mean?
		create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shader_module;
		if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
			throw std::runtime_error("Failed to create shader module.");

		return shader_module;
	}

	// https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Framebuffers
	void create_framebuffers()
	{
		swap_chain_framebuffers.resize(swap_chain_image_views.size());

		for (size_t i = 0; i < swap_chain_image_views.size(); i++)
		{
			VkImageView attachments[] = { swap_chain_image_views[i] };

			VkFramebufferCreateInfo framebuffer_create_info{};
			framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_create_info.renderPass = render_pass;
			framebuffer_create_info.attachmentCount = 1;
			framebuffer_create_info.pAttachments = attachments;
			framebuffer_create_info.width = swap_chain_extent.width;
			framebuffer_create_info.height = swap_chain_extent.height;
			framebuffer_create_info.layers = 1;

			if (vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &swap_chain_framebuffers[i]) != VK_SUCCESS)
				throw std::runtime_error("Failed to create framebuffer.");
		}
	}

	// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Command_buffers#page_Command-pools
	void create_command_pool()
	{
		QueueFamilyIndices queue_family_indices;
		find_queue_indices(physical_device, queue_family_indices);

		VkCommandPoolCreateInfo pool_create_info{};
		pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_create_info.queueFamilyIndex = queue_family_indices.graphics_family;
		pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(device, &pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
			throw std::runtime_error("Failed to create command pool.");
	}

	// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Command_buffers#page_Command-buffer-allocation
	void create_command_buffers()
	{
		command_buffers.resize(swap_chain_framebuffers.size());

		VkCommandBufferAllocateInfo alloc_create_info{};
		alloc_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_create_info.commandPool = command_pool;
		alloc_create_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_create_info.commandBufferCount = (uint32_t) command_buffers.size();

		if (vkAllocateCommandBuffers(device, &alloc_create_info, command_buffers.data()) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate command buffers.");
	}

	void record_command_buffer(int image_index)
	{
		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(command_buffers[image_index], &begin_info) != VK_SUCCESS)
			throw std::runtime_error("Failed to begin recording command buffer.");

		begin_render_pass(image_index);
	}

	// https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers#page_Starting-a-render-pass
	void begin_render_pass(int framebuffer_index)
	{
		VkRenderPassBeginInfo render_pass_begin_info{};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = render_pass;
		render_pass_begin_info.framebuffer = swap_chain_framebuffers[framebuffer_index];
		render_pass_begin_info.renderArea.offset = {0, 0};
		render_pass_begin_info.renderArea.extent = swap_chain_extent;

		VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_color;

		vkCmdBeginRenderPass(command_buffers[framebuffer_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[framebuffer_index], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

		vkCmdDraw(command_buffers[framebuffer_index], 3, 1, 0, 0);

		vkCmdEndRenderPass(command_buffers[framebuffer_index]);

		if (vkEndCommandBuffer(command_buffers[framebuffer_index]) != VK_SUCCESS)
			throw std::runtime_error("Failed to record a command buffer.");
	}

	void create_sync_objects()
	{
		image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
		in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
		images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Semaphores
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Frames-in-flight
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS
		 	 || vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS
			 || vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS)
				throw std::runtime_error("Failed to create sync objects for a frame.");
		}
	}

	void draw_frame()
	{
		vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

		// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Synchronization
		uint32_t image_index;
		VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
			recreate_swap_chain();
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error("Failed to acquire swap chain image.");

		record_command_buffer(image_index);

		// Check if the previous frame is using the image (i.e. there is its fence to wait on)
		if (images_in_flight[image_index] != VK_NULL_HANDLE)
			vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);

		// Mark the image as now being in use by this frame
		images_in_flight[image_index] = in_flight_fences[current_frame];

		https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Rendering_and_presentation#page_Submitting-the-command-buffer
		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffers[image_index];

		VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		vkResetFences(device, 1, &in_flight_fences[current_frame]);

		if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS)
			throw std::runtime_error("Failed to submit a draw command buffer.");

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swap_chains[] = { swap_chain };

		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr;

		// NOTE: graphics queue is our present queue
		result = vkQueuePresentKHR(graphics_queue, &present_info);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized)
		{
			framebuffer_resized = false;
			recreate_swap_chain();
		}
		else if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to acquire swap chain image.");

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void cleanup_swap_chain()
	{
		for (size_t i = 0; i < swap_chain_framebuffers.size(); i++)
			vkDestroyFramebuffer(device, swap_chain_framebuffers[i], nullptr);

		vkDestroyPipeline(device, graphics_pipeline, nullptr);
		
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

		vkDestroyRenderPass(device, render_pass, nullptr);

		for (size_t i = 0; i < swap_chain_image_views.size(); i++)
			vkDestroyImageView(device, swap_chain_image_views[i], nullptr);

		vkDestroySwapchainKHR(device, swap_chain, nullptr);
	}

	void cleanup()
	{
		cleanup_swap_chain();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
			vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
			vkDestroyFence(device, in_flight_fences[i], nullptr);
		}

		vkDestroyCommandPool(device, command_pool, nullptr);

		vkDestroyDevice(device, nullptr);

		// Surface destroyed before the instance
		vkDestroySurfaceKHR(instance, surface, nullptr);

		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
		app->framebuffer_resized = true;
	}

	static std::vector<char> read_file(const std::string& filename)
	{
		// We start reading at the end of the file so we can easily determine the size of the file and allocate a buffer
		std::ifstream input(filename, std::ios::ate | std::ios::binary);

		if (!input.is_open())
			throw std::runtime_error("Failed to open the file!");

		size_t file_size = (size_t) input.tellg();
		std::vector<char> buffer(file_size);

		input.seekg(0);
		input.read(buffer.data(), file_size);

		input.close();

		return buffer;
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