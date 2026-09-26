#include "volumetric_clouds.hpp"

#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/mod/init_context.hpp>
#include <RobloxModLoader/mod/mod_base.hpp>
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
	}

	void on_unload() override
	{
	}

private:
	std::shared_ptr<spdlog::logger> m_log;
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
