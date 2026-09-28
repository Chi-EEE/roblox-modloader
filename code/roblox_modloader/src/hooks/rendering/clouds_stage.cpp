#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/roblox/graphics/device_context.hpp"
#include "RobloxModLoader/roblox/graphics/global_shader_data.hpp"
#include "RobloxModLoader/roblox/graphics/render_camera.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "roblox/graphics/graphics_registry.hpp"

static RBX::Graphics::Texture* scene_depth_of(const RBX::Graphics::SceneManager* scene_manager)
{
	const auto* targets = scene_manager ? scene_manager->get_main_render_targets() : nullptr;
	const auto* framebuffer = targets ? targets->scene_fb.get() : nullptr;
	return framebuffer ? framebuffer->depth.texture.get() : nullptr;
}

void rml::Hooks::clouds_update(void* clouds, RBX::Graphics::DeviceContext* context, void* view_info, const RBX::Graphics::RenderCamera* camera, RBX::Graphics::Framebuffer* main_framebuffer, RBX::Graphics::GlobalShaderData* globals, const void* camera_change, void* stats)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	bool replaced = false;
	if (registry.validate())
	{
		auto* scene_manager = registry.scene_manager();
		graphics::RenderPassContext pass{context, main_framebuffer, registry.device(), camera, scene_manager, graphics::RenderStage::SkyPrepare, globals, scene_depth_of(scene_manager), registry.capture_mode()};
		registry.run_render_callbacks(pass);
		replaced = pass.replaces_engine_clouds;
	}
	registry.set_engine_clouds_replaced(replaced);

	if (registry.engine_clouds_enabled() && !replaced)
		Hooking::get_original<&Hooks::clouds_update>()(clouds, context, view_info, camera, main_framebuffer, globals, camera_change, stats);
}

void rml::Hooks::clouds_composite(void* clouds, RBX::Graphics::DeviceContext* context, const void* camera, RBX::Graphics::GlobalShaderData* globals, void* stats)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	if (registry.engine_clouds_enabled() && !registry.engine_clouds_replaced())
		Hooking::get_original<&Hooks::clouds_composite>()(clouds, context, camera, globals, stats);

	if (!registry.validate())
		return;

	auto* scene_manager = registry.scene_manager();
	const auto* targets = scene_manager ? scene_manager->get_main_render_targets() : nullptr;
	auto* target = targets ? targets->scene_fb.get() : nullptr;
	graphics::RenderPassContext pass{context, target, registry.device(), static_cast<const RBX::Graphics::RenderCamera*>(camera), scene_manager, graphics::RenderStage::PostOpaque, globals, scene_depth_of(scene_manager), registry.capture_mode()};

	std::size_t invoked = 0;
	if (target && context->get_framebuffer() == target && registry.has_render_callbacks(graphics::RenderStage::PostOpaque))
	{
		context->end_pass();
		invoked += registry.run_render_callbacks(pass);
		context->begin_pass(target, RBX::Graphics::PassClear::All, RBX::Graphics::PassClear::All, nullptr, nullptr, 0);
	}

	pass.stage = graphics::RenderStage::Sky;
	pass.scene_depth = nullptr;
	invoked += registry.run_render_callbacks(pass);
	if (invoked > 0 && globals)
		context->bind_buffer_data(0, globals, static_cast<unsigned>(sizeof(RBX::Graphics::GlobalShaderData)));
}

void* rml::Hooks::device_destroy(RBX::Graphics::Device* self, const unsigned int flags)
{
	graphics::GraphicsRegistry::instance().on_device_destroyed(self);
	return Hooking::get_original<&Hooks::device_destroy>()(self, flags);
}
