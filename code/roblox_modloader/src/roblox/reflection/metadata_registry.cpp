#include "metadata_registry.hpp"

#include "class_registry.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/mod/events.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"
#include "RobloxModLoader/qt/qt_integration.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "pointers.hpp"

#include <format>

RML_LOG_SCOPE("Metadata");

namespace rml::reflection
{
	static constexpr std::string_view k_metadata_file = "ReflectionMetadata.xml";
	static constexpr std::size_t k_instance_probe_size = 0x20;

	static std::string class_name_of(const RBX::Instance& instance)
	{
		return std::string(instance.get_descriptor().name.to_string());
	}

	static RBX::Instance* find_child(const RBX::Instance* parent, const auto& matches)
	{
		if (!parent || !parent->children)
			return nullptr;
		for (const auto& child : *parent->children)
		{
			if (child && matches(*child))
				return child.get();
		}
		return nullptr;
	}

	static RBX::Instance* child_named(const RBX::Instance* parent, const std::string_view name)
	{
		return find_child(parent, [name](const RBX::Instance& child) { return child.name.value() == name; });
	}

	static RBX::Instance* child_of_class(const RBX::Instance* parent, const std::string_view class_name)
	{
		return find_child(parent, [class_name](const RBX::Instance& child) { return class_name_of(child) == class_name; });
	}

	static std::optional<std::string> read(const RBX::Instance* item, const char* field)
	{
		const auto* descriptor = item ? item->get_descriptor().find_property(field) : nullptr;
		if (!descriptor)
			return std::nullopt;
		return descriptor->get_string_value(item);
	}

	static bool write(RBX::Instance* item, const char* field, const std::string& text)
	{
		const auto* descriptor = item->get_descriptor().find_property(field);
		if (!descriptor)
		{
			RML_WARN("{} has no field {}; skipped", class_name_of(*item), field);
			return false;
		}
		if (!descriptor->set_string_value(item, text))
		{
			RML_WARN("{}.{} rejected '{}'", item->name.value(), field, text);
			return false;
		}
		return true;
	}

	static bool set_parent(RBX::Instance* child, RBX::Instance* parent)
	{
		const auto* descriptor = child->get_descriptor().find_property("Parent");
		if (!descriptor)
			return false;
		static_cast<const RBX::Reflection::RefPropertyDescriptor*>(descriptor)->set_ref_value(child, parent);
		return true;
	}

	static std::shared_ptr<RBX::Instance> create(const char* class_name)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.get_string_atom || !p.object_create_by_name)
			return nullptr;
		const auto atom = p.get_string_atom(class_name);
		if (!atom)
			return nullptr;
		return p.object_create_by_name(nullptr, *reinterpret_cast<const RBX::Name*>(atom), RBX::CreatorRole::Engine);
	}

	static const char* text_of(const bool value)
	{
		return value ? "true" : "false";
	}

	static bool validate(const RBX::Instance* root)
	{
		auto* classes = child_of_class(root, "ReflectionMetadataClasses");
		auto* sound = child_named(classes, "Sound");
		auto* properties = child_of_class(sound, "ReflectionMetadataProperties");
		auto* volume = child_named(properties, "Volume");
		const auto maximum = read(volume, "UIMaximum");
		if (!maximum)
			return false;
		RML_INFO("metadata tree validated (Sound.Volume UIMaximum = {})", *maximum);
		return true;
	}

	static void apply_class(RBX::Instance* item, const RBX::Instance* classes, const ClassHints& hints)
	{
		if (hints.description)
			write(item, "Description", *hints.description);
		if (hints.insert_category)
			write(item, "ClassCategory", *hints.insert_category);
		if (hints.explorer_order)
			write(item, "ExplorerOrder", std::to_string(*hints.explorer_order));
		if (hints.preferred_parent)
			write(item, "PreferredParent", *hints.preferred_parent);
		if (hints.insertable)
			write(item, "Insertable", text_of(*hints.insertable));
		if (hints.browsable)
			write(item, "Browsable", text_of(*hints.browsable));
		if (hints.icon_of)
		{
			if (const auto index = read(child_named(classes, *hints.icon_of), "ExplorerImageIndex"); index && !index->empty())
				write(item, "ExplorerImageIndex", *index);
			else
				RML_WARN("icon_of('{}'): that class has no ExplorerImageIndex", *hints.icon_of);
		}
	}

	static void apply_property(RBX::Instance* member, const PropertyHints& hints)
	{
		if (hints.description)
			write(member, "Description", *hints.description);
		if (hints.order)
			write(member, "PropertyOrder", std::to_string(*hints.order));
		if (hints.read_only)
			write(member, "EditingDisabled", "true");
		if (hints.hidden)
			write(member, "Browsable", "false");
		if (hints.deprecated)
		{
			write(member, "Deprecated", "true");
			if (!hints.description && !hints.deprecated->empty())
				write(member, "Description", *hints.deprecated);
		}
		if (const auto& slider = hints.slider)
		{
			write(member, "UIMinimum", std::format("{}", slider->min));
			write(member, "UIMaximum", std::format("{}", slider->max));
			if (slider->ticks > 0)
				write(member, "UINumTicks", std::to_string(slider->ticks));
			if (slider->scaling == SliderScaling::Square)
				write(member, "SliderScaling", "Square");
		}
	}

	MetadataRegistry& MetadataRegistry::instance()
	{
		static MetadataRegistry registry;
		return registry;
	}

	void MetadataRegistry::add(ClassMetadata metadata)
	{
		std::call_once(m_install_once, [this] {
			events::event_manager().register_handler<events::DataModelChangedEvent>([this](events::DataModelChangedEvent&) { request_flush(); });
		});
		{
			std::lock_guard lock(m_mutex);
			m_pending.push_back(std::move(metadata));
		}
		request_flush();
	}

	void MetadataRegistry::request_flush()
	{
		if (m_disabled.load(std::memory_order_acquire))
			return;
		{
			std::lock_guard lock(m_mutex);
			if (m_pending.empty())
				return;
		}
		if (auto* qt = qt::QtIntegration::instance())
			qt->run_on_gui_thread([this] { flush(); });
	}

	void MetadataRegistry::disable(const std::string& reason)
	{
		if (!m_disabled.exchange(true, std::memory_order_acq_rel))
			RML_ERROR("mod metadata disabled: {}", reason);
	}

	RBX::Instance* MetadataRegistry::find_root()
	{
		if (!m_getter)
		{
			const auto functions = memory::functions_referencing_string(k_metadata_file);
			if (functions.size() != 1)
			{
				disable(std::format("{} function(s) reference \"{}\"; expected only the metadata singleton getter", functions.size(), k_metadata_file));
				return nullptr;
			}
			m_getter = functions.front();
		}

		const auto* metadata_class = ClassRegistry::instance().find_engine_class("ReflectionMetadata");
		if (!metadata_class)
		{
			disable("the ReflectionMetadata class is not registered");
			return nullptr;
		}

		const memory::module image(platform::studio_image_name());
		for (auto* reference : memory::data_references_from(*m_getter))
		{
			if (!image.contains(memory::handle(reference)) || reinterpret_cast<std::uintptr_t>(reference) % alignof(void*) != 0)
				continue;
			auto* candidate = *static_cast<RBX::Instance* const*>(reference);
			if (!candidate || reinterpret_cast<std::uintptr_t>(candidate) % alignof(void*) != 0 || !platform::is_readable(candidate, k_instance_probe_size))
				continue;
			if (&candidate->get_descriptor() == metadata_class)
				return candidate;
		}
		return nullptr;
	}

	void MetadataRegistry::flush()
	{
		if (m_disabled.load(std::memory_order_acquire))
			return;

		if (!m_root)
		{
			auto* root = find_root();
			if (!root)
				return;
			if (!validate(root))
			{
				disable("the ReflectionMetadata tree does not have the expected shape");
				return;
			}
			m_root = root;
			RML_INFO("ReflectionMetadata singleton at 0x{:X}", reinterpret_cast<std::uintptr_t>(root));
		}

		std::vector<ClassMetadata> pending;
		{
			std::lock_guard lock(m_mutex);
			pending.swap(m_pending);
		}
		for (const auto& metadata : pending)
		{
			try
			{
				apply(metadata);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("metadata for {} failed: {}", metadata.class_name, e.what());
			}
			catch (...)
			{
				RML_ERROR("metadata for {} failed with an unknown exception", metadata.class_name);
			}
		}
	}

	RBX::Instance* MetadataRegistry::insert(RBX::Instance* parent, const char* class_name, const std::string& name)
	{
		auto created = create(class_name);
		if (!created)
		{
			RML_ERROR("could not create a {}", class_name);
			return nullptr;
		}
		write(created.get(), "Name", name);
		if (!set_parent(created.get(), parent))
		{
			RML_ERROR("could not parent {} '{}'", class_name, name);
			return nullptr;
		}
		auto* raw = created.get();
		m_owned.push_back(std::move(created));
		return raw;
	}

	void MetadataRegistry::apply(const ClassMetadata& metadata)
	{
		auto* classes = child_of_class(m_root, "ReflectionMetadataClasses");
		auto* item = child_named(classes, metadata.class_name);
		if (!item)
			item = insert(classes, "ReflectionMetadataClass", metadata.class_name);
		if (!item)
			return;

		if (metadata.owned_class)
			apply_class(item, classes, metadata.hints);

		std::size_t applied = 0;
		if (!metadata.properties.empty())
		{
			auto* properties = child_of_class(item, "ReflectionMetadataProperties");
			if (!properties)
				properties = insert(item, "ReflectionMetadataProperties", "ReflectionMetadataProperties");
			if (!properties)
				return;

			for (const auto& property : metadata.properties)
			{
				auto* member = child_named(properties, property.name);
				if (member && !metadata.owned_class)
				{
					RML_WARN("{}.{} already has engine metadata; left unchanged", metadata.class_name, property.name);
					continue;
				}
				if (!member)
					member = insert(properties, "ReflectionMetadataMember", property.name);
				if (!member)
					continue;
				apply_property(member, property.hints);
				++applied;
			}
		}

		RML_INFO("metadata applied for {} ({} member(s))", metadata.class_name, applied);
	}
}
