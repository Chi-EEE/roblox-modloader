#include "RobloxModLoader/platform/graphics/shader_compiler.hpp"

#include "RobloxModLoader/memory/module.hpp"

#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <format>

namespace rml::platform
{
	class D3DCompiler
	{
	public:
		static const D3DCompiler& instance()
		{
			static const D3DCompiler compiler;
			return compiler;
		}

		[[nodiscard]] pD3DCompile compile() const noexcept
		{
			return m_compile;
		}

	private:
		D3DCompiler()
		{
			if (m_module.loaded() || m_module.attach())
				m_compile = m_module.get_export("D3DCompile").as<pD3DCompile>();
		}

		memory::module m_module{std::string_view("d3dcompiler_47.dll")};
		pD3DCompile m_compile{};
	};

	static std::string_view profile_version(const std::string_view shading_language)
	{
		if (shading_language == "d3d11")
			return "5_0";
		if (shading_language == "d3d10_1")
			return "4_1";
		return "4_0";
	}

	static std::string_view stage_prefix(const RBX::Graphics::Shader::Type stage)
	{
		switch (stage)
		{
		case RBX::Graphics::Shader::Type::Vertex: return "vs_";
		case RBX::Graphics::Shader::Type::Fragment: return "ps_";
		case RBX::Graphics::Shader::Type::Compute: return "cs_";
		}
		return "vs_";
	}

	std::expected<std::vector<char>, std::string> compile_shader(const graphics::ShaderSource& source, const RBX::Graphics::Shader::Type stage, const std::string_view shading_language)
	{
		if (!shading_language.starts_with("d3d"))
			return std::unexpected(std::format("shading language '{}' is not supported on Windows", shading_language));
		if (source.hlsl.empty())
			return std::unexpected("no HLSL source for the D3D backend");

		const auto compile = D3DCompiler::instance().compile();
		if (!compile)
			return std::unexpected("d3dcompiler_47.dll is not available");

		const auto target = std::format("{}{}", stage_prefix(stage), profile_version(shading_language));
		const std::string entry_point(source.entry_point);

		Microsoft::WRL::ComPtr<ID3DBlob> code;
		Microsoft::WRL::ComPtr<ID3DBlob> errors;
		const auto result = compile(source.hlsl.data(), source.hlsl.size(), nullptr, nullptr, nullptr, entry_point.c_str(), target.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
		if (FAILED(result))
		{
			if (errors)
				return std::unexpected(std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
			return std::unexpected(std::format("D3DCompile failed with 0x{:08X}", static_cast<unsigned>(result)));
		}

		const auto* bytes = static_cast<const char*>(code->GetBufferPointer());
		return std::vector<char>(bytes, bytes + code->GetBufferSize());
	}
}
