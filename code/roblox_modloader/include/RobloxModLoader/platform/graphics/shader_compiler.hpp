#pragma once

#include "RobloxModLoader/roblox/graphics/shader.hpp"
#include "RobloxModLoader/roblox/graphics/shader_source.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace rml::platform
{
	[[nodiscard]] std::expected<std::vector<char>, std::string> compile_shader(const graphics::ShaderSource& source, RBX::Graphics::Shader::Type stage, std::string_view shading_language);
}
