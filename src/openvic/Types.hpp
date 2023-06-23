#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "utility/Logger.hpp"

namespace OpenVic {
	// Represents a 24-bit RGB integer OR a 32-bit ARGB integer
	using colour_t = uint32_t;
	/* When colour_t is used as an identifier, NULL_COLOUR is disallowed
	 * and should be reserved as an error value.
	 * When colour_t is used in a purely graphical context, NULL_COLOUR
	 * should be allowed.
	 */
	static constexpr colour_t NULL_COLOUR = 0, FULL_COLOUR = 0xFF, MAX_COLOUR_RGB = 0xFFFFFF;
	constexpr colour_t float_to_colour_byte(float f, float min = 0.0f, float max = 1.0f) {
		return static_cast<colour_t>(std::clamp(min + f * (max - min), min, max) * 255.0f);
	}
	constexpr colour_t fraction_to_colour_byte(int n, int d, float min = 0.0f, float max = 1.0f) {
		return float_to_colour_byte(static_cast<float>(n) / static_cast<float>(d), min, max);
	}
	constexpr colour_t float_to_alpha_value(float a) {
		return float_to_colour_byte(a) << 24;
	}
	constexpr colour_t rgba_to_colour(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
		return (a << 24) | (r << 16) | (g << 8) | b;
	}

	using index_t = uint16_t;
	static constexpr index_t NULL_INDEX = 0, MAX_INDEX = 0xFFFF;

	using ovstring = std::wstring;

	// TODO: price_t must be changed to a fixed-point numeric type before multiplayer
	using price_t = double;
	static constexpr price_t NULL_PRICE = 0.0;
	using return_t = bool;

	// This mirrors godot::Error, where `OK = 0` and `FAILED = 1`.
	static constexpr return_t SUCCESS = false, FAILURE = true;

	/*
	 * Base class for objects with a non-empty string identifier,
	 * uniquely named instances of which can be entered into an
	 * IdentifierRegistry instance.
	 */
	class HasIdentifier {
		const ovstring identifier;

	protected:
		HasIdentifier(ovstring const& new_identifier);

	public:
		HasIdentifier(HasIdentifier const&) = delete;
		HasIdentifier(HasIdentifier&&) = default;
		HasIdentifier& operator=(HasIdentifier const&) = delete;
		HasIdentifier& operator=(HasIdentifier&&) = delete;

		ovstring const& get_identifier() const;
	};

	/*
	 * Base class for objects with associated colour information
	 */
	class HasColour {
		const colour_t colour;

	protected:
		HasColour(colour_t const new_colour, bool can_be_null = false);

	public:
		HasColour(HasColour const&) = delete;
		HasColour(HasColour&&) = default;
		HasColour& operator=(HasColour const&) = delete;
		HasColour& operator=(HasColour&&) = delete;

		colour_t get_colour() const;
		ovstring colour_to_hex_string() const;
		static ovstring colour_to_hex_string(colour_t const colour);
	};

	/*
	 * Template for a list of objects with unique string identifiers that can
	 * be locked to prevent any further additions. The template argument T is
	 * the type of object that the registry will store, and the second part ensures
	 * that HasIdentifier is a base class of T.
	 */
	template<class T, typename std::enable_if<std::is_base_of<HasIdentifier, T>::value>::type* = nullptr>
	class IdentifierRegistry {
		using identifier_index_map_t = std::map<ovstring, size_t>;

		const std::string name;
		std::vector<T> items;
		bool locked = false;
		identifier_index_map_t identifier_index_map;

	public:
		IdentifierRegistry(std::string const& new_name) : name(new_name) {}
		return_t add_item(T&& item) {
			if (locked) {
				// Logger::error("Cannot add item to the ", name, " registry - locked!");
				return FAILURE;
			}
			T const* old_item = get_item_by_identifier(item.get_identifier());
			if (old_item != nullptr) {
				// Logger::error("Cannot add item to the ", name, " registry - an item with the identifier \"", item.get_identifier(), "\" already exists!");
				return FAILURE;
			}
			identifier_index_map[item.get_identifier()] = items.size();
			items.push_back(std::move(item));
			return SUCCESS;
		}
		void lock(bool log = true) {
			if (locked) {
				// Logger::error("Failed to lock ", name, " registry - already locked!");
			} else {
				locked = true;
				// if (log) Logger::info("Locked ", name, " registry after registering ", get_item_count(), " items");
			}
		}
		bool is_locked() const {
			return locked;
		}
		void reset() {
			identifier_index_map.clear();
			items.clear();
			locked = false;
		}
		size_t get_item_count() const {
			return items.size();
		}
		T* get_item_by_identifier(ovstring const& identifier) {
			const identifier_index_map_t::const_iterator it = identifier_index_map.find(identifier);
			if (it != identifier_index_map.end()) return &items[it->second];
			return nullptr;
		}
		T const* get_item_by_identifier(ovstring const& identifier) const {
			const identifier_index_map_t::const_iterator it = identifier_index_map.find(identifier);
			if (it != identifier_index_map.end()) return &items[it->second];
			return nullptr;
		}
		T* get_item_by_index(size_t index) {
			return index < items.size() ? &items[index] : nullptr;
		}
		T const* get_item_by_index(size_t index) const {
			return index < items.size() ? &items[index] : nullptr;
		}
		std::vector<T>& get_items() {
			return items;
		}
		std::vector<T> const& get_items() const {
			return items;
		}
	};
}
