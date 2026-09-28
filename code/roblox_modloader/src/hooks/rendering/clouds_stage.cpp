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

static const RBX::Graphics::RenderCamera* g_sky_camera = nullptr;

static void run_sky_stages(RBX::Graphics::DeviceContext* context, const RBX::Graphics::RenderCamera* camera, RBX::Graphics::GlobalShaderData* globals)
{
	auto& registry = rml::graphics::GraphicsRegistry::instance();
	if (!registry.validate())
		return;

	auto* scene_manager = registry.scene_manager();
	const auto* targets = scene_manager ? scene_manager->get_main_render_targets() : nullptr;
	auto* target = targets ? targets->scene_fb.get() : nullptr;
	rml::graphics::RenderPassContext pass{context, target, registry.device(), camera, scene_manager, rml::graphics::RenderStage::PostOpaque, globals, scene_depth_of(scene_manager), registry.capture_mode()};

	std::size_t invoked = 0;
	if (target && context->get_framebuffer() == target && registry.has_render_callbacks(rml::graphics::RenderStage::PostOpaque))
	{
		const auto* open = registry.open_pass(target);
		const auto resumed = open ? std::optional(*open) : std::nullopt;
		context->end_pass();
		invoked += registry.run_render_callbacks(pass);
		if (resumed)
			context->begin_pass(target, resumed->store_mask, resumed->store_mask, nullptr, resumed->resolves ? &resumed->resolve : nullptr, resumed->flags);
		else
			context->begin_pass(target, target->mask, target->mask, nullptr, nullptr, 0);
	}

	pass.stage = rml::graphics::RenderStage::Sky;
	pass.scene_depth = nullptr;
	invoked += registry.run_render_callbacks(pass);
	if (invoked > 0 && globals)
		context->bind_buffer_data(0, globals, static_cast<unsigned>(sizeof(RBX::Graphics::GlobalShaderData)));
}

void rml::Hooks::clouds_update(void* clouds, RBX::Graphics::DeviceContext* context, void* view_info, const RBX::Graphics::RenderCamera* camera, RBX::Graphics::Framebuffer* main_framebuffer, RBX::Graphics::GlobalShaderData* globals, const void* camera_change, void* stats)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	g_sky_camera = camera;
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
	run_sky_stages(context, static_cast<const RBX::Graphics::RenderCamera*>(camera), globals);
}

void rml::Hooks::clouds_composite_clouds(void* clouds, RBX::Graphics::DeviceContext* context, RBX::Graphics::GlobalShaderData* globals, void* stats)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	if (registry.engine_clouds_enabled() && !registry.engine_clouds_replaced())
		Hooking::get_original<&Hooks::clouds_composite_clouds>()(clouds, context, globals, stats);
	run_sky_stages(context, g_sky_camera, globals);
}

void* rml::Hooks::device_destroy(RBX::Graphics::Device* self, const unsigned int flags)
{
	graphics::GraphicsRegistry::instance().on_device_destroyed(self);
	return Hooking::get_original<&Hooks::device_destroy>()(self, flags);
}

void rml::Hooks::device_context_begin_pass(RBX::Graphics::DeviceContext* self, RBX::Graphics::Framebuffer* framebuffer, const unsigned load_mask, unsigned store_mask, const RBX::Graphics::PassClear* clear, const RBX::Graphics::PassResolve* resolve, const unsigned flags)
{
	store_mask = graphics::GraphicsRegistry::instance().on_begin_pass(framebuffer, store_mask, resolve, flags);
	Hooking::get_original<&Hooks::device_context_begin_pass>()(self, framebuffer, load_mask, store_mask, clear, resolve, flags);
}
