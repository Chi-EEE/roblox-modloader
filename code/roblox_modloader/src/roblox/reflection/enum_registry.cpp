#include "enum_registry.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/roblox/reflection/property_accessor.hpp"
#include "app/init_gate.hpp"
#include "pointers.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <limits>
#include <set>

RML_LOG_SCOPE("EnumRegistry");

namespace rml::reflection
{
	using RBX::Reflection::EnumDescriptor;
	using RBX::Reflection::Variant;

	static bool holds_enum_value(const EnumDescriptor* self, const Variant& value)
	{
		return !value.is_void() && (&value.type() == self || &value.type() == RBX::Reflection::Type::try_singleton<int>());
	}

	static void destroy_enum(void*)
	{
	}

	static const EnumDescriptor::Item* lookup_in_variant(const EnumDescriptor* self, const Variant& value)
	{
		return holds_enum_value(self, value) ? self->find_item_by_value(*value.try_cast<int>()) : nullptr;
	}

	static void variant_to_int(const EnumDescriptor* self, const Variant& value, int& out)
	{
		if (holds_enum_value(self, value))
			out = *value.try_cast<int>();
	}

	static void int_to_variant(const EnumDescriptor* self, const int value, Variant& out)
	{
		if (!self->find_item_by_value(value))
			return;
		if (const auto* ops = static_cast<const void* const*>(out.value_ops()); ops && ops != VariantOps<int>::table)
			reinterpret_cast<void (*)(char*)>(const_cast<void*>(ops[2]))(static_cast<char*>(out.storage()));
		out.set_type_and_ops(self, VariantOps<int>::table);
		*out.try_cast<int>() = value;
	}

	static void* const* engine_enum_donor()
	{
		static void* const* donor = [] {
			auto* const index = memory::rtti();
			const auto found = index ? index->find_matching("RBX::Reflection::EnumDesc<*") : std::nullopt;
			return found ? static_cast<void* const*>(*found) : nullptr;
		}();
		return donor;
	}

	static bool valid_name(const std::string_view name)
	{
		return !name.empty() && !std::isdigit(static_cast<unsigned char>(name.front())) &&
		       std::ranges::all_of(name, [](const char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
	}

	static std::expected<void, std::string> check_items(const EnumSpec& spec)
	{
		if (spec.items.empty())
			return std::unexpected(std::format("enum '{}' has no items", spec.name));
		if (spec.items.size() > std::numeric_limits<std::uint16_t>::max())
			return std::unexpected(std::format("enum '{}' has too many items", spec.name));
		std::set<std::string_view> names;
		std::set<std::uint32_t> hashes;
		std::set<int> values;
		for (const auto& item : spec.items)
		{
			if (!valid_name(item.name))
				return std::unexpected(std::format("enum '{}': item name '{}' must be an identifier", spec.name, item.name));
			if (!names.insert(item.name).second)
				return std::unexpected(std::format("enum '{}': item '{}' is declared twice", spec.name, item.name));
			if (!hashes.insert(EnumDescriptor::name_hash(item.name)).second)
				return std::unexpected(std::format("enum '{}': item '{}' collides with another item's name hash; rename it", spec.name, item.name));
			if (!values.insert(item.value).second)
				return std::unexpected(std::format("enum '{}': value {} of item '{}' is used twice", spec.name, item.value, item.name));
		}
		return {};
	}

	EnumRegistry& EnumRegistry::instance()
	{
		static auto* registry = new EnumRegistry();
		return *registry;
	}

	bool EnumRegistry::available()
	{
		if (!g_pointers)
			return false;
		const auto& p = g_pointers->m_roblox_pointers;
		return p.enum_descriptor_ctor && p.enum_item_ctor && p.enum_descriptor_lookup;
	}

	const EnumDescriptor* EnumRegistry::find_engine(const std::string_view name)
	{
		if (!available())
			return nullptr;
		const std::string text(name);
		return static_cast<const EnumDescriptor*>(g_pointers->m_roblox_pointers.enum_descriptor_lookup(text.c_str()));
	}

	const EnumDescriptor* EnumRegistry::find(const void* key) const
	{
		std::lock_guard lock(m_mutex);
		const auto it = m_by_key.find(key);
		return it == m_by_key.end() ? nullptr : it->second;
	}

	std::expected<const EnumDescriptor*, std::string> EnumRegistry::commit(const EnumSpec& spec)
	{
		if (!available())
			return std::unexpected("enum registration is unavailable on this build");
		if (!g_init_gate || !g_init_gate->is_open())
			return std::unexpected("enums can only be registered inside on_init");
		if (!valid_name(spec.name))
			return std::unexpected(std::format("enum name '{}' must be an identifier", spec.name));
		if (find(spec.key))
			return std::unexpected(std::format("the C++ type behind enum '{}' is already registered", spec.name));
		if (auto checked = check_items(spec); !checked)
			return std::unexpected(checked.error());

		auto result = spec.bind ? bind(spec) : define(spec);
		if (!result)
			return result;
		std::lock_guard lock(m_mutex);
		m_by_key[spec.key] = *result;
		return result;
	}

	std::expected<const EnumDescriptor*, std::string> EnumRegistry::bind(const EnumSpec& spec)
	{
		const auto* engine = find_engine(spec.name);
		if (!engine)
			return std::unexpected(std::format("bind_enum: engine enum '{}' does not exist", spec.name));
		for (const auto& item : spec.items)
		{
			if (!item.hints.empty())
				return std::unexpected(std::format("bind_enum('{}'): item '{}' cannot carry hints; the engine owns its metadata", spec.name, item.name));
			const auto* found = engine->find_item_by_name(item.name);
			if (!found || found->name.to_string() != item.name)
				return std::unexpected(std::format("bind_enum('{}'): the engine enum has no item '{}'", spec.name, item.name));
			if (found->value != item.value)
				return std::unexpected(std::format("bind_enum('{}'): item '{}' is {} in the engine, not {}", spec.name, item.name, found->value, item.value));
		}
		RML_INFO("Bound enum {} ({} item(s) declared)", spec.name, spec.items.size());
		return engine;
	}

	std::expected<const EnumDescriptor*, std::string> EnumRegistry::define(const EnumSpec& spec)
	{
		if (find_engine(spec.name))
			return std::unexpected(std::format("enum '{}' already exists", spec.name));
		const auto* donor = engine_enum_donor();
		if (!donor)
			return std::unexpected("no engine EnumDesc vtable in the RTTI index");

		const auto& p = g_pointers->m_roblox_pointers;
		auto& storage = *m_enums.emplace_back(std::make_unique<ModEnum>());
		auto* bytes = storage.descriptor.data();
		p.enum_descriptor_ctor(bytes, spec.name.c_str());
		auto* const* engine_vtable = *reinterpret_cast<void* const* const*>(bytes);

		const auto count = spec.items.size();
		storage.items = std::make_unique<std::byte[]>(count * sizeof(EnumDescriptor::Item));
		for (std::size_t i = 0; i < count; ++i)
			p.enum_item_ctor(storage.items.get() + i * sizeof(EnumDescriptor::Item), spec.items[i].name.c_str(), RBX::Reflection::Descriptor::Attributes{}, spec.items[i].value, bytes);

		for (std::size_t i = 0; i < count; ++i)
			storage.by_name.push_back({EnumDescriptor::name_hash(spec.items[i].name), static_cast<std::uint16_t>(i), EnumDescriptor::no_alias});
		std::ranges::sort(storage.by_name, {}, &EnumDescriptor::NameEntry::hash);

		bool dense = true;
		for (std::size_t i = 0; i < count; ++i)
			dense = dense && spec.items[i].value == static_cast<int>(i);
		if (!dense)
		{
			for (std::size_t i = 0; i < count; ++i)
				storage.by_value.push_back({spec.items[i].value, static_cast<std::uint16_t>(i), 0});
			std::ranges::sort(storage.by_value, {}, &EnumDescriptor::ValueEntry::value);
		}

		auto& descriptor = *reinterpret_cast<EnumDescriptor*>(bytes);
		descriptor.items = reinterpret_cast<const EnumDescriptor::Item*>(storage.items.get());
		descriptor.item_count = count;
		descriptor.by_name = storage.by_name.data();
		descriptor.by_name_count = storage.by_name.size();
		descriptor.by_value = dense ? nullptr : storage.by_value.data();
		descriptor.by_value_count = dense ? 0 : storage.by_value.size();

		storage.vtable[0] = engine_vtable[-2];
		storage.vtable[1] = engine_vtable[-1];
		storage.vtable[2] = reinterpret_cast<void*>(&destroy_enum);
		storage.vtable[3] = reinterpret_cast<void*>(&destroy_enum);
		storage.vtable[4] = donor[2];
		storage.vtable[5] = donor[3];
		storage.vtable[6] = donor[4];
		storage.vtable[7] = reinterpret_cast<void*>(&lookup_in_variant);
		storage.vtable[8] = reinterpret_cast<void*>(&variant_to_int);
		storage.vtable[9] = reinterpret_cast<void*>(&int_to_variant);
		*reinterpret_cast<void**>(bytes) = &storage.vtable[2];

		RML_INFO("Registered enum {} ({} item(s), {} lookup, descriptor 0x{:X})", spec.name, count, dense ? "dense" : "sorted", reinterpret_cast<std::uintptr_t>(bytes));
		return &descriptor;
	}
}
