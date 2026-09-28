#include "cloud_renderer.hpp"
#include "volumetric_clouds.hpp"

#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/mod/init_context.hpp>
#include <RobloxModLoader/mod/mod_base.hpp>
#include <RobloxModLoader/roblox/graphics/render_pass.hpp>
#include <spdlog/spdlog.h>

#include <memory>

class VolumetricCloudsMod final : public ModBase
{
public:
	VolumetricCloudsMod()
	{
		name = "Volumetric Clouds";
		version = "1.0.0";
		author = "Revolution";
		description = "Adds the VolumetricClouds instance";
		m_log = rml::Logger::get_logger("VolumetricClouds");
	}

	void on_init(rml::InitContext& context) override
	{
		clouds::VolumetricClouds::define(context);
		m_log->info("VolumetricClouds registered");
	}

	void on_load() override
	{
		using rml::graphics::RenderPassContext;
		using rml::graphics::RenderStage;
		m_teardown = rml::graphics::add_device_teardown_callback([this](RBX::Graphics::Device& device) {
			if (m_renderer)
				m_renderer->on_device_destroyed(device);
		});
		m_renderer = std::make_unique<clouds::CloudRenderer>(paths().dir("cache") / "noise.bin", m_log, m_teardown != 0);
		m_prepare = rml::graphics::add_render_callback(RenderStage::SkyPrepare, [this](RenderPassContext& pass) { m_renderer->prepare(pass); });
		m_render = rml::graphics::add_render_callback(RenderStage::PostOpaque, [this](RenderPassContext& pass) { m_renderer->render(pass); });
		m_composite = rml::graphics::add_render_callback(RenderStage::Sky, [this](RenderPassContext& pass) { m_renderer->composite(pass); });
	}

	void on_unload() override
	{
		rml::graphics::remove_render_callback(m_prepare);
		rml::graphics::remove_render_callback(m_render);
		rml::graphics::remove_render_callback(m_composite);
		rml::graphics::remove_render_callback(m_teardown);
		rml::graphics::set_sky_stage_enabled(false);
		m_renderer.reset();
	}

private:
	std::shared_ptr<spdlog::logger> m_log;
	std::unique_ptr<clouds::CloudRenderer> m_renderer;
	rml::graphics::RenderCallbackId m_prepare{};
	rml::graphics::RenderCallbackId m_render{};
	rml::graphics::RenderCallbackId m_composite{};
	rml::graphics::RenderCallbackId m_teardown{};
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new VolumetricCloudsMod();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()
