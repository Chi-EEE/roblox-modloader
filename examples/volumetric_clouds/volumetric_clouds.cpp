#include "volumetric_clouds.hpp"

#include <lua.h>
#include <lualib.h>

namespace clouds
{
	VolumetricClouds::VolumetricClouds()
	{
		CloudRegistry::instance().add(this, m_settings);
	}

	VolumetricClouds::~VolumetricClouds()
	{
		CloudRegistry::instance().remove(this);
	}

	bool VolumetricClouds::placed_in_world() const
	{
		for (const RBX::Instance* node = parent; node; node = node->parent)
		{
			const auto name = node->get_class_name().to_string();
			if (name == "Lighting" || name == "Workspace")
				return true;
		}
		return false;
	}

	void VolumetricClouds::on_ancestor_changed(const RBX::AncestorChanged& change)
	{
		call_engine_base(&RBX::Instance::on_ancestor_changed, change);
		CloudRegistry::instance().set_placed(this, placed_in_world());
	}

	int VolumetricClouds::apply_preset(lua_State* L)
	{
		const int top = lua_gettop(L);
		const char* name = top > 0 && lua_isstring(L, top) ? lua_tolstring(L, top, nullptr) : nullptr;
		if (!name || !m_settings.apply_preset(name))
			luaL_errorL(L, "ApplyPreset expects \"Sparse\", \"Cloudy\", \"Overcast\" or \"Stormy\"");
		CloudRegistry::instance().update(this, m_settings);
		return 0;
	}

	void VolumetricClouds::define(rml::InitContext& context)
	{
		using Self = VolumetricClouds;
		using S = CloudSettings;
		context.define_class<Self>("VolumetricClouds")
		    .property("Enabled", &Self::get<&S::enabled>, &Self::set<&S::enabled>, "Behavior")
		    .property("Coverage", &Self::get<&S::coverage>, &Self::set<&S::coverage>, "Shape")
		    .property("Density", &Self::get<&S::density>, &Self::set<&S::density>, "Shape")
		    .property("CloudType", &Self::get<&S::cloud_type>, &Self::set<&S::cloud_type>, "Shape")
		    .property("ShapeFactor", &Self::get<&S::shape_factor>, &Self::set<&S::shape_factor>, "Shape")
		    .property("ShapeScale", &Self::get<&S::shape_scale>, &Self::set<&S::shape_scale>, "Shape")
		    .property("ErosionFactor", &Self::get<&S::erosion_factor>, &Self::set<&S::erosion_factor>, "Shape")
		    .property("ErosionScale", &Self::get<&S::erosion_scale>, &Self::set<&S::erosion_scale>, "Shape")
		    .property("BaseAltitude", &Self::get<&S::base_altitude>, &Self::set<&S::base_altitude>, "Layer")
		    .property("Thickness", &Self::get<&S::thickness>, &Self::set<&S::thickness>, "Layer")
		    .property("HorizonFade", &Self::get<&S::horizon_fade>, &Self::set<&S::horizon_fade>, "Layer")
		    .property("WindDirection", &Self::get<&S::wind_direction>, &Self::set<&S::wind_direction>, "Wind")
		    .property("WindSpeed", &Self::get<&S::wind_speed>, &Self::set<&S::wind_speed>, "Wind")
		    .property("Color", &Self::get<&S::color>, &Self::set<&S::color>, "Lighting")
		    .property("SunIntensity", &Self::get<&S::sun_intensity>, &Self::set<&S::sun_intensity>, "Lighting")
		    .property("AmbientIntensity", &Self::get<&S::ambient_intensity>, &Self::set<&S::ambient_intensity>, "Lighting")
		    .property("Powder", &Self::get<&S::powder>, &Self::set<&S::powder>, "Lighting")
		    .property("MultiScattering", &Self::get<&S::multi_scattering>, &Self::set<&S::multi_scattering>, "Lighting")
		    .property("Quality", &Self::get<&S::quality>, &Self::set<&S::quality>, "Behavior")
		    .property("Seed", &Self::get<&S::seed>, &Self::set<&S::seed>, "Behavior")
		    .function("ApplyPreset", &Self::apply_preset)
		    .commit();
	}
}
