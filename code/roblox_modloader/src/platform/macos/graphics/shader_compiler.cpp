#include "RobloxModLoader/platform/graphics/shader_compiler.hpp"

namespace rml::platform
{
	std::expected<std::vector<char>, std::string> compile_shader(const graphics::ShaderSource& source, RBX::Graphics::Shader::Type, std::string_view)
	{
		if (source.metal.empty())
			return std::unexpected("no Metal source for the Metal backend");
		return std::vector<char>(source.metal.begin(), source.metal.end());
	}
}
