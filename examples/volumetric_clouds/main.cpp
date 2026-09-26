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
		m_renderer = std::make_unique<clouds::CloudRenderer>(paths().dir("cache") / "noise.bin", m_log);
		m_prepare = rml::graphics::add_render_callback(RenderStage::SkyPrepare, [this](RenderPassContext& pass) { m_renderer->prepare(pass); });
		m_composite = rml::graphics::add_render_callback(RenderStage::Sky, [this](RenderPassContext& pass) { m_renderer->composite(pass); });
	}

	void on_unload() override
	{
		rml::graphics::remove_render_callback(m_prepare);
		rml::graphics::remove_render_callback(m_composite);
		rml::graphics::set_sky_stage_enabled(false);
		rml::graphics::set_engine_clouds_hidden(false);
		m_renderer.reset();
	}

private:
	std::shared_ptr<spdlog::logger> m_log;
	std::unique_ptr<clouds::CloudRenderer> m_renderer;
	rml::graphics::RenderCallbackId m_prepare{};
	rml::graphics::RenderCallbackId m_composite{};
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
