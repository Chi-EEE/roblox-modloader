#pragma once

#include "RobloxModLoader/memory/string_anchor.hpp"
#include "RobloxModLoader/roblox/reflection/metadata.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RBX
{
	class Instance;
}

namespace rml::reflection
{
	struct PropertyMetadata
	{
		std::string name;
		PropertyHints hints;
	};

	struct ClassMetadata
	{
		std::string class_name;
		bool owned_class{};
		ClassHints hints;
		std::vector<PropertyMetadata> properties;
	};

	class MetadataRegistry
	{
	public:
		static MetadataRegistry& instance();

		void add(ClassMetadata metadata);
		void request_flush();
		void on_tree_loaded(RBX::Instance* root);
		void register_category(std::string_view name);
		[[nodiscard]] std::optional<std::string> category_name(std::string_view translation_suffix) const;

	private:
		void flush();
		bool adopt_root(RBX::Instance* root);
		RBX::Instance* find_root();
		void disable(const std::string& reason);
		void apply(const ClassMetadata& metadata);
		RBX::Instance* insert(RBX::Instance* parent, const char* class_name, const std::string& name);
		void apply_descriptions();

		std::mutex m_mutex;
		std::vector<ClassMetadata> m_pending;
		std::once_flag m_install_once;
		std::atomic<bool> m_disabled{false};
		std::optional<memory::AnchoredFunction> m_getter;
		RBX::Instance* m_root{};
		std::vector<std::shared_ptr<RBX::Instance>> m_owned;
		std::unordered_map<std::string, std::string> m_descriptions;
		std::optional<memory::AnchoredFunction> m_fill_documentation;
		bool m_descriptions_pending{};
		bool m_descriptions_unavailable{};
		mutable std::mutex m_categories_mutex;
		std::unordered_map<std::string, std::string> m_categories;
	};
}
