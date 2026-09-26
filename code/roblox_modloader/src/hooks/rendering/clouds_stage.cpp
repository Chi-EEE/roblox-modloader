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
	if (registry.engine_clouds_visible())
		Hooking::get_original<&Hooks::clouds_update>()(clouds, context, view_info, camera, main_framebuffer, globals, camera_change, stats);

	if (!registry.validate())
		return;

	auto* scene_manager = registry.scene_manager();
	graphics::RenderPassContext pass{context, main_framebuffer, registry.device(), camera, scene_manager, graphics::RenderStage::SkyPrepare, globals, scene_depth_of(scene_manager), registry.capture_mode()};
	registry.run_render_callbacks(pass);
}

void rml::Hooks::clouds_composite(void* clouds, RBX::Graphics::DeviceContext* context, const void* camera, RBX::Graphics::GlobalShaderData* globals, void* stats)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	if (registry.engine_clouds_visible())
		Hooking::get_original<&Hooks::clouds_composite>()(clouds, context, camera, globals, stats);

	if (!registry.validate())
		return;

	auto* scene_manager = registry.scene_manager();
	const auto* targets = scene_manager ? scene_manager->get_main_render_targets() : nullptr;
	graphics::RenderPassContext pass{context, targets ? targets->scene_fb.get() : nullptr, registry.device(), static_cast<const RBX::Graphics::RenderCamera*>(camera), scene_manager, graphics::RenderStage::Sky, globals, nullptr, registry.capture_mode()};
	if (registry.run_render_callbacks(pass) > 0 && globals)
		context->bind_buffer_data(0, globals, static_cast<unsigned>(sizeof(RBX::Graphics::GlobalShaderData)));
}
