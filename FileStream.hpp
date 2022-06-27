#pragma once

#include <vector>
#include <fstream>

class FileStream
{
public:
	static std::vector<char> read_file(const std::string& filename);
};
