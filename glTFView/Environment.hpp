#pragma once

#include <Tsuki/Common.hpp>
#include <filesystem>

class Environment {
 public:
	Environment(tk::Device& device, const std::filesystem::path& envPath);

	tk::ImageHandle Skybox;
	tk::ImageHandle Irradiance;
	tk::ImageHandle Prefiltered;
	tk::ImageHandle BrdfLut;
};
