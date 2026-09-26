#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace RBX
{
	class Adorn;
}

namespace RBX::Graphics
{
	class Device;
	class DeviceContext;
	class Framebuffer;
	class IShaderManager;
	class ShaderProgram;
	class AdornRender;
	class RenderCamera;
	class SceneManager;
	class Texture;
	class VisualEngine;
	struct GlobalShaderData;
}

namespace rml::graphics
{
	enum class RenderStage : std::uint8_t
	{
		SkyPrepare,
		Sky,
		Scene
	};

	struct RenderPassContext
	{
		RBX::Graphics::DeviceContext* context;
		RBX::Graphics::Framebuffer* target;
		RBX::Graphics::Device* device;
		const RBX::Graphics::RenderCamera* camera;
		RBX::Graphics::SceneManager* scene_manager;
		RenderStage stage{RenderStage::Scene};
		const RBX::Graphics::GlobalShaderData* globals{};
		RBX::Graphics::Texture* scene_depth{};
		std::uint32_t capture_mode{};
	};

	using RenderCallback = std::function<void(RenderPassContext&)>;
	using RenderCallbackId = std::uint64_t;
	using AdornCallback = std::function<void(RBX::Adorn&)>;

	RML_EXPORT RBX::Graphics::VisualEngine* visual_engine();
	RML_EXPORT RBX::Graphics::Device* device();
	RML_EXPORT RBX::Graphics::SceneManager* scene_manager();
	RML_EXPORT void add_render_callback(RenderCallback callback);
	RML_EXPORT RenderCallbackId add_render_callback(RenderStage stage, RenderCallback callback);
	RML_EXPORT void remove_render_callback(RenderCallbackId id);
	RML_EXPORT void set_sky_stage_enabled(bool enabled);
	RML_EXPORT void set_engine_clouds_hidden(bool hidden);
	RML_EXPORT void add_adorn_callback(AdornCallback callback);
	RML_EXPORT RBX::Graphics::AdornRender* adorn_render();
	RML_EXPORT std::vector<RBX::Graphics::AdornRender*> adorn_renders();
	RML_EXPORT RBX::Graphics::IShaderManager* shader_manager();
	RML_EXPORT std::shared_ptr<RBX::Graphics::ShaderProgram> engine_program(std::string_view vertex, std::string_view fragment);
}
