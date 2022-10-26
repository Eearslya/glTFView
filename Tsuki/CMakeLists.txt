cmake_minimum_required(VERSION 3.21)
project(Tsuki LANGUAGES CXX)

include(FetchContent)
include(FindVulkan)
add_subdirectory(External)

option(TSUKI_DEBUG_VULKAN "Enable debugging and validation for Tsuki Vulkan runtime" OFF)
option(TSUKI_INCLUDE_GLFW "Include default GLFW platform" ON)

add_library(Tsuki STATIC)

if (TSUKI_DEBUG_VULKAN)
	target_compile_definitions(Tsuki PRIVATE TSUKI_DEBUG_VULKAN)
	target_link_libraries(Tsuki PRIVATE spdlog)
endif()

if (TSUKI_INCLUDE_GLFW)
	FetchContent_Declare(glfw
		GIT_REPOSITORY https://github.com/glfw/glfw.git
		GIT_TAG c50d53160fa9b579dda0d0a4f9a7c2512940df8e)
	set(GLFW_BUILD_DOCS Off CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES Off CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS Off CACHE BOOL "" FORCE)
	set(GLFW_INSTALL Off CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(glfw)

	target_compile_definitions(Tsuki PRIVATE TSUKI_INCLUDE_GLFW)
	target_link_libraries(Tsuki PRIVATE glfw)
endif()

FetchContent_Declare(glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG cc98465e3508535ba8c7f6208df934c156a018dc)
FetchContent_Declare(glslang
	GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
	GIT_TAG 7e6b7c26a2a565c677b93f1c1769977cadced306)
FetchContent_Declare(spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG 834840636cb52242d0fae2d6233c90067a4248dc)
FetchContent_Declare(spirv-cross
	GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
	GIT_TAG d8d051381f65b9606fb8016c79b7c3bab872eec3)

set(ENABLE_GLSLANG_BINARIES Off CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI Off CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL On CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL Off CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL Off CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS Off CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm glslang spdlog spirv-cross)

set_property(TARGET Tsuki PROPERTY CXX_STANDARD 20)
target_compile_definitions(Tsuki
	PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_QUAT_DATA_XYZW VK_ENABLE_BETA_EXTENSIONS VK_NO_PROTOTYPES
	PRIVATE VMA_NULLABLE= VMA_NOT_NULL= VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)
target_include_directories(Tsuki
	PUBLIC Include
	PRIVATE Source)
target_link_libraries(Tsuki
	PUBLIC imgui Vulkan::Headers
	PRIVATE glslang SPIRV spirv-cross-cpp)

add_subdirectory(Source)
