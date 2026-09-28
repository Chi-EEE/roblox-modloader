#include "metadata_registry.hpp"

#include "class_registry.hpp"
#include "app/init_gate.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/mod/events.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"
#include "RobloxModLoader/qt/qt_integration.hpp"
#include "RobloxModLoader/roblox/instance.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "pointers.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <set>

RML_LOG_SCOPE("Metadata");

namespace rml::reflection
{
	static constexpr std::string_view k_metadata_file = "ReflectionMetadata.xml";
	static constexpr std::size_t k_instance_probe_size = 0x20;
	static constexpr std::string_view k_documentation_log = "[FLog::ReflectionMetadata] {} has no translated description!";
	static constexpr std::string_view k_documentation_prefix = "@roblox/globaltype/";

	struct DocumentationSource
	{
		std::string (*lookup)(void* context, const std::string& key);
		void* context;
	};

	using fill_documentation = void (*)(RBX::Instance* classes, DocumentationSource source);

	static std::string lookup_description(void* context, const std::string& key)
	{
		try
		{
			const auto& descriptions = *static_cast<const std::unordered_map<std::string, std::string>*>(context);
			const auto it = descriptions.find(key);
			return it == descriptions.end() ? std::string{} : it->second;
		}
		catch (...)
		{
			return {};
		}
	}

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
			static std::mutex reported_mutex;
			static std::set<std::string, std::less<>> reported;
			std::lock_guard lock(reported_mutex);
			if (reported.emplace(std::format("{}.{}", class_name_of(*item), field)).second)
				RML_WARN("{} has no field {}; that hint is ignored on this build", class_name_of(*item), field);
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
		if (hints.insert_category)
			write(item, "ClassCategory", *hints.insert_category);
		if (hints.explorer_order)
			write(item, "ExplorerOrder", std::to_string(*hints.explorer_order));
		if (hints.preferred_parent)
			write(item, "PreferredParent", *hints.preferred_parent);
		write(item, "Insertable", text_of(hints.insertable.value_or(true)));
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
		if (hints.order)
			write(member, "PropertyOrder", std::to_string(*hints.order));
		if (hints.read_only)
			write(member, "EditingDisabled", "true");
		if (hints.hidden)
			write(member, "Browsable", "false");
		if (hints.deprecated)
			write(member, "Deprecated", "true");
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
		if (g_init_gate && g_init_gate->is_open())
			flush();
		else
			request_flush();
	}

	static std::string translation_suffix_of(const std::string_view name)
	{
		std::string suffix;
		for (const auto c : name)
		{
			if (std::isalnum(static_cast<unsigned char>(c)))
				suffix.push_back(c);
		}
		return suffix;
	}

	void MetadataRegistry::register_category(const std::string_view name)
	{
		auto suffix = translation_suffix_of(name);
		if (suffix.empty())
			return;
		std::lock_guard lock(m_categories_mutex);
		m_categories.try_emplace(std::move(suffix), name);
	}

	std::optional<std::string> MetadataRegistry::category_name(const std::string_view translation_suffix) const
	{
		std::lock_guard lock(m_categories_mutex);
		const auto it = m_categories.find(std::string(translation_suffix));
		return it == m_categories.end() ? std::nullopt : std::optional(it->second);
	}

	void MetadataRegistry::on_tree_loaded(RBX::Instance* root)
	{
		if (m_disabled.load(std::memory_order_acquire) || !root)
			return;
		if (!m_root && !adopt_root(root))
			return;
		flush();
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

	bool MetadataRegistry::adopt_root(RBX::Instance* root)
	{
		if (!validate(root))
		{
			disable("the ReflectionMetadata tree does not have the expected shape");
			return false;
		}
		m_root = root;
		RML_INFO("ReflectionMetadata singleton at 0x{:X}", reinterpret_cast<std::uintptr_t>(root));
		return true;
	}

	void MetadataRegistry::flush()
	{
		if (m_disabled.load(std::memory_order_acquire))
			return;

		if (!m_root)
		{
			auto* root = find_root();
			if (!root || !adopt_root(root))
				return;
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
		apply_descriptions();
	}

	void MetadataRegistry::apply_descriptions()
	{
		if (!m_descriptions_pending || m_descriptions_unavailable)
			return;
		m_descriptions_pending = false;

		if (!m_fill_documentation)
		{
			const auto all_classes = reinterpret_cast<void*>(g_pointers->m_roblox_pointers.class_descriptor_all_classes);
			for (const auto& candidate : memory::functions_referencing_string(k_documentation_log))
			{
				const auto calls = memory::calls_from(candidate);
				if (std::ranges::find(calls, all_classes) == calls.end())
					continue;
				if (m_fill_documentation)
				{
					m_fill_documentation.reset();
					break;
				}
				m_fill_documentation = candidate;
			}
			if (!m_fill_documentation)
			{
				m_descriptions_unavailable = true;
				RML_WARN("descriptions are unavailable on this build: the class documentation filler was not found");
				return;
			}
		}

		auto* classes = child_of_class(m_root, "ReflectionMetadataClasses");
		reinterpret_cast<fill_documentation>(m_fill_documentation->start)(classes, DocumentationSource{&lookup_description, &m_descriptions});
		RML_INFO("descriptions applied ({} entries)", m_descriptions.size());
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
		const auto created_before = m_owned.size();
		auto* classes = child_of_class(m_root, "ReflectionMetadataClasses");
		auto* item = child_named(classes, metadata.class_name);
		if (!item)
			item = insert(classes, "ReflectionMetadataClass", metadata.class_name);
		if (!item)
			return;

		if (metadata.owned_class)
		{
			apply_class(item, classes, metadata.hints);
			if (metadata.hints.description)
			{
				m_descriptions[std::format("{}{}", k_documentation_prefix, metadata.class_name)] = *metadata.hints.description;
				m_descriptions_pending = true;
			}
		}

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
				if (!member)
					member = insert(properties, "ReflectionMetadataMember", property.name);
				if (!member)
					continue;
				apply_property(member, property.hints);
				const auto& text = property.hints.description ? property.hints.description : property.hints.deprecated;
				if (text && !text->empty())
				{
					m_descriptions[std::format("{}{}.{}", k_documentation_prefix, metadata.class_name, property.name)] = *text;
					m_descriptions_pending = true;
				}
				++applied;
			}
		}

		RML_INFO("metadata applied for {} ({} member(s), {} new item(s))", metadata.class_name, applied, m_owned.size() - created_before);
	}
}
