#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/roblox/graphics/render_camera.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "roblox/graphics/graphics_registry.hpp"

void rml::Hooks::scene_manager_render_scene(void* self, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* target, const void* camera, RBX::ArrayView<RBX::Graphics::Framebuffer*> extra, std::uint32_t capture_mode)
{
	auto& registry = graphics::GraphicsRegistry::instance();
	auto* scene_manager = static_cast<RBX::Graphics::SceneManager*>(self);
	const bool engine_clouds = scene_manager->clouds_enabled;
	const bool force = !engine_clouds && registry.sky_stage_forced() && registry.validate();
	registry.begin_scene(scene_manager, engine_clouds, capture_mode);
	if (force)
		scene_manager->clouds_enabled = true;

	Hooking::get_original<&Hooks::scene_manager_render_scene>()(self, context, target, camera, extra, capture_mode);

	if (force)
		scene_manager->clouds_enabled = engine_clouds;

	if (!registry.validate())
		return;

	graphics::RenderPassContext pass{context, target, registry.device(), static_cast<const RBX::Graphics::RenderCamera*>(camera), scene_manager, graphics::RenderStage::Scene, &scene_manager->read_global_shader_data(), nullptr, capture_mode};
	registry.run_render_callbacks(pass);
}
