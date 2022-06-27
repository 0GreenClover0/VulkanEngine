#include "FileStream.hpp"

std::vector<char> FileStream::read_file(const std::string& filename)
{
	// We start reading at the end of the file so we can easily determine the size of the file and allocate a buffer
	std::ifstream input(filename, std::ios::ate | std::ios::binary);

	if (!input.is_open())
	{
		throw std::runtime_error("Failed to open the file!");
	}

	size_t file_size = (size_t)input.tellg();
	std::vector<char> buffer(file_size);

	input.seekg(0);
	input.read(buffer.data(), file_size);

	input.close();

	return buffer;
}
