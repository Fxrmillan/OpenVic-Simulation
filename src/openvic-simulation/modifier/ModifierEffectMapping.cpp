#include "ModifierEffectMapping.hpp"

#include "openvic-simulation/modifier/ModifierEffect.hpp"
#include "openvic-simulation/utility/Logger.hpp"

using namespace OpenVic;

std::string_view ModifierEffectMapping::modifier_effect_mapping_type_to_string(modifier_effect_mapping_type_t type) {
	using enum ModifierEffectMapping::modifier_effect_mapping_type_t;

	switch (type) {
#define _CASE(X) case X: return #X;
	_CASE(LEADER_MAPPING)
	_CASE(TECHNOLOGY_MAPPING)
	_CASE(UNIT_TERRAIN_MAPPING)
	_CASE(BASE_COUNTRY_MAPPING)
	_CASE(BASE_PROVINCE_MAPPING)
	_CASE(EVENT_MAPPING)
	_CASE(TERRAIN_MAPPING)
#undef _CASE
	default: return "INVALID MODIFIER TYPE";
	}
}

ModifierEffectMapping::ModifierEffectMapping(
	modifier_effect_mapping_type_t new_type, ModifierEffectMapping const* new_fallback_mapping
) : type { new_type }, locked { false }, fallback_mapping { new_fallback_mapping }, effect_map {} {}

std::string_view ModifierEffectMapping::get_type_name() const {
	return modifier_effect_mapping_type_to_string(type);
}

bool ModifierEffectMapping::add_modifier_effect(ModifierEffect const& effect) {
	if (locked) {
		Logger::error("Cannot add modifier effect to modifier effect mapping \"", get_type_name(), "\" - locked!");
		return false;
	}

	const auto [effect_it, effect_was_added] = effect_map.emplace(effect.get_mapping_key(), &effect);

	if (!effect_was_added) {
		Logger::error(
			"Cannot add modifier effect \"", effect.get_identifier(), "\" to modifier effect mapping \"", get_type_name(),
			"\" - the key \"", effect.get_mapping_key(), "\" is already mapped to modifier effect \"",
			effect_it->second->get_identifier(), "\"!"
		);
	}

	return effect_was_added;
}

void ModifierEffectMapping::lock() {
	if (locked) {
		Logger::error("Cannot lock modifier effect mapping \"", get_type_name(), "\" - already locked!");
	} else {
		locked = true;
	}
}

ModifierEffect const* ModifierEffectMapping::lookup_modifier_effect(std::string_view identifier) const {
	if (!locked) {
		Logger::error(
			"Cannot lookup modifier effect \"", identifier, "\" in modifier effect mapping \"", get_type_name(),
			"\" - not locked!"
		);
		return nullptr;
	}

	const decltype(effect_map)::const_iterator it = effect_map.find(identifier);

	if (it != effect_map.end()) {
		return it->second;
	}

	if (fallback_mapping != nullptr) {
		return fallback_mapping->lookup_modifier_effect(identifier);
	}

	return nullptr;
}
