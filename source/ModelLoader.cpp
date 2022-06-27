#include "ModelLoader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void ModelLoader::load_model(std::string model_path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str()))
		throw std::runtime_error(warn + err);

	for (const auto& shape : shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices{};

		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.tex_coord = {
				attrib.texcoords[2 * index.texcoord_index + 0],

				// The OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image,
				// however we've uploaded our image into Vulkan in a top to bottom orientation where 0 means the top of the image.
				// Solve this by flipping the vertical component of the texture coordinates.
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (unique_vertices.count(vertex) == 0)
			{
				unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(unique_vertices[vertex]);
		}
	}
}