#pragma once

#include <string_view>

#include "openvic-simulation/types/OrderedContainers.hpp"
#include "openvic-simulation/utility/Getters.hpp"

namespace OpenVic {
	struct ModifierManager;
	struct ModifierEffect;

	struct ModifierEffectMapping {
		enum struct modifier_effect_mapping_type_t : uint8_t {
			LEADER_MAPPING,        // Isolated
			TECHNOLOGY_MAPPING,    // Isolated
			UNIT_TERRAIN_MAPPING,  // Isolated
			BASE_COUNTRY_MAPPING,  // Fallen back to by BASE_PROVINCE_MAPPING, and in turn by EVENT_MAPPING and TERRAIN_MAPPING
			BASE_PROVINCE_MAPPING, // Falls back to BASE_COUNTRY_MAPPING, fallen back to by EVENT_MAPPING and TERRAIN_MAPPING
			EVENT_MAPPING,         // Falls back to BASE_PROVINCE_MAPPING
			TERRAIN_MAPPING,       // Falls back to BASE_PROVINCE_MAPPING
			MODIFIER_EFFECT_MAPPING_COUNT
		};

		static std::string_view modifier_effect_mapping_type_to_string(modifier_effect_mapping_type_t type);

	private:
		const modifier_effect_mapping_type_t PROPERTY(type);
		bool PROPERTY_CUSTOM_PREFIX(locked, is);
		ModifierEffectMapping const* const PROPERTY(fallback_mapping);
		string_map_t<ModifierEffect const*> PROPERTY(effect_map);

	public:
		ModifierEffectMapping(
			modifier_effect_mapping_type_t new_type, ModifierEffectMapping const* new_fallback_mapping = nullptr
		);
		ModifierEffectMapping(ModifierEffectMapping&&) = default;

		std::string_view get_type_name() const;

		bool add_modifier_effect(ModifierEffect const& effect);
		void lock();

		ModifierEffect const* lookup_modifier_effect(std::string_view identifier) const;
	};
}
