#include "Modifier.hpp"

using namespace OpenVic;
using namespace OpenVic::NodeTools;

ModifierEffect::ModifierEffect(std::string_view new_identifier, bool new_positive_good, format_t new_format)
	: HasIdentifier { new_identifier }, positive_good { new_positive_good }, format { new_format } {}

bool ModifierEffect::get_positive_good() const {
	return positive_good;
}

ModifierEffect::format_t ModifierEffect::get_format() const {
	return format;
}

ModifierValue::ModifierValue() = default;
ModifierValue::ModifierValue(effect_map_t&& new_values) : values { std::move(new_values) } {}
ModifierValue::ModifierValue(ModifierValue const&) = default;
ModifierValue::ModifierValue(ModifierValue&&) = default;

ModifierValue& ModifierValue::operator=(ModifierValue const&) = default;
ModifierValue& ModifierValue::operator=(ModifierValue&&) = default;

void ModifierValue::trim() {
	std::erase_if(values, [](effect_map_t::value_type const& value) -> bool {
		return value.second == fixed_point_t::_0();
	});
}

size_t ModifierValue::get_effect_count() const {
		return values.size();
}

fixed_point_t ModifierValue::get_effect(ModifierEffect const* effect, bool* successful) {
	const effect_map_t::const_iterator it = values.find(effect);
	if (it != values.end()) {
		if (successful != nullptr) *successful = true;
		return it->second;
	}
	if (successful != nullptr) *successful = false;
	return fixed_point_t::_0();
}

bool ModifierValue::has_effect(ModifierEffect const* effect) const {
	return values.find(effect) != values.end();
}

ModifierValue& ModifierValue::operator+=(ModifierValue const& right) {
	for (effect_map_t::value_type const& value : right.values) {
		values[value.first] += value.second;
	}
	return *this;
}

ModifierValue ModifierValue::operator+(ModifierValue const& right) const {
	ModifierValue ret = *this;
	return ret += right;
}

ModifierValue ModifierValue::operator-() const {
	ModifierValue ret = *this;
	for (effect_map_t::value_type& value : ret.values) {
		value.second = -value.second;
	}
	return ret;
}

ModifierValue& ModifierValue::operator-=(ModifierValue const& right) {
	for (effect_map_t::value_type const& value : right.values) {
		values[value.first] -= value.second;
	}
	return *this;
}

ModifierValue ModifierValue::operator-(ModifierValue const& right) const {
	ModifierValue ret = *this;
	return ret -= right;
}

Modifier::Modifier(std::string_view new_identifier, ModifierValue&& new_values, icon_t new_icon)
	: HasIdentifier { new_identifier }, ModifierValue { std::move(new_values) }, icon { new_icon } {}

Modifier::icon_t Modifier::get_icon() const {
	return icon;
}

ModifierInstance::ModifierInstance(Modifier const& modifier, Date expiry_date) : modifier { modifier }, expiry_date { expiry_date } {}

Modifier const& ModifierInstance::get_modifier() const {
	return modifier;
}

Date const& ModifierInstance::get_expiry_date() const {
	return expiry_date;
}

ModifierManager::ModifierManager()
	: modifier_effects { "modifier effects"}, modifiers { "modifiers" } {}

bool ModifierManager::add_modifier_effect(std::string_view identifier, bool positive_good, ModifierEffect::format_t format) {
	if (identifier.empty()) {
		Logger::error("Invalid modifier effect identifier - empty!");
		return false;
	}
	return modifier_effects.add_item({ identifier, positive_good, format });
}

bool ModifierManager::add_modifier(std::string_view identifier, ModifierValue&& values, Modifier::icon_t icon) {
	if (identifier.empty()) {
		Logger::error("Invalid modifier effect identifier - empty!");
		return false;
	}
	if (icon <= 0) {
		Logger::error("Invalid modifier icon for ", identifier, ": ", icon);
		return false;
	}
	return modifiers.add_item({ identifier, std::move(values), icon });
}

bool ModifierManager::setup_modifier_effects() {
	bool ret = true;

	using enum ModifierEffect::format_t;

	ret &= add_modifier_effect("movement_cost", false);
	ret &= add_modifier_effect("farm_rgo_size", true);
	ret &= add_modifier_effect("farm_rgo_eff", true);
	ret &= add_modifier_effect("mine_rgo_size", true);
	ret &= add_modifier_effect("mine_rgo_eff", true);
	ret &= add_modifier_effect("supply_limit", true, RAW_DECIMAL);
	ret &= add_modifier_effect("combat_width", false);
	ret &= add_modifier_effect("defence", true, RAW_DECIMAL);
	ret &= add_modifier_effect("local_ship_build", false);
	ret &= add_modifier_effect("research_points_modifier", true);
	ret &= add_modifier_effect("local_rgo_output", true);
	ret &= add_modifier_effect("attrition", false, RAW_DECIMAL);
	ret &= add_modifier_effect("immigrant_push", false);
	ret &= add_modifier_effect("population_growth", true);
	ret &= add_modifier_effect("local_RGO_throughput", true);
	ret &= add_modifier_effect("assimilation_rate", true);

	/* These should be added automatically for each Building loaded (or at least
	 * non-factories), however currently we need modifier effects locked before we
	 * can load buildings, so some architectural changes will be needed.
	 */
	ret &= add_modifier_effect("max_fort", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("min_build_fort", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("max_naval_base", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("min_build_naval_base", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("max_railroad", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("min_build_railroad", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("max_university", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("min_build_university", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("max_bank", true, ModifierEffect::format_t::INT);
	ret &= add_modifier_effect("min_build_bank", true, ModifierEffect::format_t::INT);
		
	modifier_effects.lock();
	return ret;
}

node_callback_t ModifierManager::expect_modifier_value_and_default(callback_t<ModifierValue&&> modifier_callback, key_value_callback_t default_callback) const {
	return [this, modifier_callback, default_callback](ast::NodeCPtr root) -> bool {
		ModifierValue modifier;
		bool ret = expect_dictionary(
			[this, &modifier, default_callback](std::string_view key, ast::NodeCPtr value) -> bool {
				ModifierEffect const* effect = get_modifier_effect_by_identifier(key);
				if (effect != nullptr) {
					if (modifier.values.find(effect) == modifier.values.end()) {
						return expect_fixed_point(
							assign_variable_callback(modifier.values[effect])
						)(value);
					}
					Logger::error("Duplicate modifier effect: ", key);
					return false;
				}
				return default_callback(key, value);
			}
		)(root);
		ret &= modifier_callback(std::move(modifier));
		return ret;
	};
}

node_callback_t ModifierManager::expect_modifier_value(callback_t<ModifierValue&&> modifier_callback) const {
	return expect_modifier_value_and_default(modifier_callback, key_value_invalid_callback);
}

node_callback_t ModifierManager::expect_modifier_value_and_key_map_and_default(callback_t<ModifierValue&&> modifier_callback, key_value_callback_t default_callback, key_map_t&& key_map) const {
	return [this, modifier_callback, key_map = std::move(key_map)](ast::NodeCPtr node) mutable -> bool {
		bool ret = expect_modifier_value_and_default(
			modifier_callback, dictionary_keys_callback(key_map, key_value_invalid_callback)
		)(node);
		ret &= check_key_map_counts(key_map);
		return ret;
	};
}

node_callback_t ModifierManager::expect_modifier_value_and_key_map(callback_t<ModifierValue&&> modifier_callback, key_map_t&& key_map) const {
	return expect_modifier_value_and_key_map_and_default(modifier_callback, key_value_invalid_callback, std::move(key_map));
}

namespace OpenVic { //so the compiler shuts up
	std::ostream& operator<<(std::ostream& stream, ModifierValue const& value) {
		for (ModifierValue::effect_map_t::value_type const& effect : value.values) {
			stream << effect.first << ": " << effect.second << "\n";
		}
		return stream;
	}
}
