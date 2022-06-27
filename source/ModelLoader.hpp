#pragma once

#include <array>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static VkVertexInputBindingDescription get_binding_description()
	{
		VkVertexInputBindingDescription binding_description{};
		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, color);

		attribute_descriptions[2].binding = 0;
		attribute_descriptions[2].location = 2;
		attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[2].offset = offsetof(Vertex, tex_coord);

		return attribute_descriptions;
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tex_coord) << 1);
		}
	};
}

struct Uniform_Buffer_Object
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

// TODO: change this name
class ModelLoader
{
public:
	static void load_model(std::string model_path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
};
