#include "RobloxModLoader/roblox/graphics/shader_source.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/graphics/shader_compiler.hpp"
#include "RobloxModLoader/roblox/graphics/device.hpp"

#include <cstring>
#include <format>

RML_LOG_SCOPE("Graphics");

namespace rml::graphics
{
	std::vector<char> make_shader_blob(const std::string_view payload, const std::uint64_t buffer_mask, const std::uint32_t reserved)
	{
		std::vector<char> blob(sizeof(buffer_mask) + sizeof(reserved) + payload.size());
		std::memcpy(blob.data(), &buffer_mask, sizeof(buffer_mask));
		std::memcpy(blob.data() + sizeof(buffer_mask), &reserved, sizeof(reserved));
		std::memcpy(blob.data() + sizeof(buffer_mask) + sizeof(reserved), payload.data(), payload.size());
		return blob;
	}

	static std::expected<std::shared_ptr<RBX::Graphics::Shader>, std::string> create_shader(RBX::Graphics::Device& device, const ShaderSource& source, const RBX::Graphics::Shader::Type stage, const std::string_view language, const std::string& name)
	{
		const auto payload = platform::compile_shader(source, stage, language);
		if (!payload)
			return std::unexpected(std::format("{}: {}", name, payload.error()));

		auto shader = device.create_shader(stage, make_shader_blob({payload->data(), payload->size()}), name);
		if (!shader)
			return std::unexpected(std::format("{}: the device rejected the shader", name));
		return shader;
	}

	std::expected<std::shared_ptr<RBX::Graphics::ShaderProgram>, std::string> create_program(RBX::Graphics::Device& device, const ShaderSource& vertex, const ShaderSource& fragment, const std::string& name)
	{
		using namespace RBX::Graphics;
		try
		{
			const auto language = device.get_shading_language();

			auto vs = create_shader(device, vertex, Shader::Type::Vertex, language, name + ".vs");
			if (!vs)
				return std::unexpected(vs.error());
			auto fs = create_shader(device, fragment, Shader::Type::Fragment, language, name + ".fs");
			if (!fs)
				return std::unexpected(fs.error());

			const std::shared_ptr<Shader> shaders[2] = {std::move(*vs), std::move(*fs)};
			auto program = device.create_shader_program(shaders, 2, name);
			if (!program)
				return std::unexpected(std::format("{}: the device rejected the program", name));
			return program;
		}
		catch (const std::exception& e)
		{
			return std::unexpected(std::format("{}: {}", name, e.what()));
		}
	}
}
