cmake_minimum_required(VERSION 3.1)

project(VulkanEngine LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(Vulkan REQUIRED)
find_package(GLFW REQUIRED)
find_package(glm REQUIRED)

add_executable(VulkanEngine "VulkanEngine.cpp")

target_link_libraries(VulkanEngine PRIVATE
Vulkan::Vulkan
GLFW::GLFW
glm::glm
)