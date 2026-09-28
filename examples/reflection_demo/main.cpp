#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/mod/init_context.hpp>
#include <RobloxModLoader/mod/mod_base.hpp>
#include <RobloxModLoader/roblox/reflection/described_creatable.hpp>
#include <RobloxModLoader/roblox/util/G3DCore.h>
#include <lua.h>
#include <spdlog/spdlog.h>

#include <string>

static std::shared_ptr<spdlog::logger> g_log;
static int g_workspace_counter;

static int workspace_ping(RBX::Instance*, lua_State* L)
{
	lua_pushstring(L, "pong");
	return 1;
}

static int get_workspace_counter(RBX::Instance*)
{
	return g_workspace_counter;
}

static void set_workspace_counter(RBX::Instance*, const int& value)
{
	g_workspace_counter = value;
}

class ModThing final : public rml::reflection::DescribedCreatable<ModThing>
{
public:
	float speed{};
	double volume{0.5};
	int count{};
	bool enabled{true};
	RBX::Color3 tint{1.f, 1.f, 1.f};
	RBX::Vector3 offset;
	std::string label{"ModThing"};
	std::string notes;
	int legacy{};
	bool debug{};
	rbx::signal<void(float)> speed_reset;

	int reset(lua_State* L)
	{
		const auto previous = speed;
		speed = 0.f;
		label = "reset";
		fire(&ModThing::speed_reset, previous);
		lua_pushnumber(L, previous);
		return 1;
	}

	void on_child_added(RBX::Instance* child) override
	{
		g_log->info("on_child_added: {} <- {}", label, child->name.value());
	}
};

class ModMarker final : public rml::reflection::DescribedCreatable<ModMarker>
{
public:
	std::string tag;
};

class reflection_demo final : public ModBase
{
public:
	reflection_demo()
	{
		name = "Reflection Demo";
		version = "0.1.0";
		author = "RML";
		description = "Registers ModThing through on_init";
		g_log = rml::Logger::get_logger("ReflectionDemo");
	}

	void on_load() override
	{
		g_log->info("loaded");
	}

	void on_init(rml::InitContext& context) override
	{
		using rml::reflection::SliderScaling;
		context.define_class<ModThing>("ModThing")
		    .description("Example class from reflection_demo. It shows every Properties panel option the loader supports.")
		    .insert_category("RML")
		    .explorer_order(1)
		    .preferred_parent("Workspace")
		    .icon_of("Folder")
		    .insertable(true)
		    .property("Enabled", &ModThing::enabled).category("Behavior").order(0)
		    .property("Speed", &ModThing::speed).category("Motion").order(10).slider(0.f, 100.f, 100).description("Units per second. A linear slider with 100 steps.")
		    .property("Volume", &ModThing::volume).category("Motion").order(11).slider(0.0, 10.0, 1000, SliderScaling::Square).description("A square-scaled slider, like Sound.Volume.")
		    .property("Count", &ModThing::count).category("Motion").order(12).slider(0, 20, 20)
		    .property("Tint", &ModThing::tint).category("Appearance").order(20)
		    .property("Offset", &ModThing::offset).category("Appearance").order(21)
		    .property("Label", &ModThing::label).category("Data").order(30).read_only().description("Read-only in the Properties panel; scripts can still write it.")
		    .property("Notes", &ModThing::notes).category("Data").order(31)
		    .property("Legacy", &ModThing::legacy).category("Data").order(32).deprecated("Use Speed instead.")
		    .property("Debug", &ModThing::debug).hidden()
		    .function("Reset", &ModThing::reset)
		    .event("SpeedReset", &ModThing::speed_reset, {"previous"})
		    .commit();
		context.define_class<ModMarker>("ModMarker")
		    .description("Script-only class: not offered by Insert Object.")
		    .insert_category("RML")
		    .icon_of("Configuration")
		    .insertable(false)
		    .property("Tag", &ModMarker::tag).description("Free-form tag.")
		    .commit();
		context.extend_class("Workspace")
		    .function("RmlPing", &workspace_ping)
		    .property("RmlCounter", &get_workspace_counter, &set_workspace_counter).category("RML").slider(0, 10, 10).description("Counter added to Workspace by reflection_demo.")
		    .commit();
		g_log->info("ModThing registered ({} bytes)", sizeof(ModThing));
	}

	void on_unload() override
	{
	}
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new reflection_demo();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()
