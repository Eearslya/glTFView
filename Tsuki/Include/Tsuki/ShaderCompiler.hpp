#pragma once

#include <optional>

#include "Common.hpp"

namespace tk {
class ShaderCompiler {
 public:
	ShaderCompiler();
	~ShaderCompiler() noexcept;

	std::optional<std::vector<uint32_t>> Compile(vk::ShaderStageFlagBits stage, const std::string& glsl) const;
};
}  // namespace tk
