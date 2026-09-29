#pragma once

#include "descriptor.hpp"
#include "type.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace RBX::Reflection
{
	class EnumDescriptor : public Type
	{
	public:
		class Item : public Descriptor
		{
		public:
			const EnumDescriptor* owner;
			int value;

			Item() = delete;
		};

		struct NameEntry
		{
			std::uint32_t hash;
			std::uint16_t item;
			std::uint16_t alias;
		};

		struct ValueEntry
		{
			std::int32_t value;
			std::uint16_t item;
			std::uint16_t reserved_6;
		};

		static constexpr std::uint16_t no_alias = 0xFFFF;

		const Item* items;
		std::size_t item_count;
		const NameEntry* by_name;
		std::size_t by_name_count;
		const ValueEntry* by_value;
		std::size_t by_value_count;
		const char* const* aliases;
		std::size_t alias_count;

		EnumDescriptor() = delete;

		[[nodiscard]] static constexpr std::uint32_t name_hash(const std::string_view name)
		{
			std::uint32_t hash = 0x811C9DC5u;
			for (const auto c : name)
			{
				hash ^= static_cast<unsigned char>(c);
				hash *= 0x01000193u;
			}
			return hash;
		}

		[[nodiscard]] std::span<const Item> all_items() const
		{
			return {items, item_count};
		}

		[[nodiscard]] const Item* find_item_by_value(int value) const;
		[[nodiscard]] const Item* find_item_by_name(std::string_view name) const;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
#if !defined(RML_WINDOWS)
	RML_ASSERT_SIZE(Type, 0x40);
	RML_ASSERT_OFFSET(Type, type_id, 0x30);
	RML_ASSERT_OFFSET(Type, is_enum, 0x36);
	RML_ASSERT_SIZE(EnumDescriptor, 0x80);
	RML_ASSERT_OFFSET(EnumDescriptor, items, 0x40);
	RML_ASSERT_OFFSET(EnumDescriptor, by_name, 0x50);
	RML_ASSERT_OFFSET(EnumDescriptor, by_value, 0x60);
	RML_ASSERT_OFFSET(EnumDescriptor, aliases, 0x70);
	RML_ASSERT_SIZE(EnumDescriptor::Item, 0x38);
	RML_ASSERT_OFFSET(EnumDescriptor::Item, owner, 0x28);
	RML_ASSERT_OFFSET(EnumDescriptor::Item, value, 0x30);
	RML_ASSERT_SIZE(EnumDescriptor::NameEntry, 0x8);
	RML_ASSERT_SIZE(EnumDescriptor::ValueEntry, 0x8);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
