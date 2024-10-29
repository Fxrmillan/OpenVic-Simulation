#pragma once

#include <variant>
#include <vector>

#include "openvic-simulation/dataloader/NodeTools.hpp"
#include "openvic-simulation/scripts/ConditionScript.hpp"
#include "openvic-simulation/types/fixed_point/FixedPoint.hpp"

namespace OpenVic {
	struct ConditionalWeight {
		using condition_weight_t = std::pair<fixed_point_t, ConditionScript>;
		using condition_weight_group_t = std::vector<condition_weight_t>;
		using condition_weight_item_t = std::variant<condition_weight_t, condition_weight_group_t>;

		enum class base_key_t : uint8_t {
			BASE, FACTOR, MONTHS
		};
		using enum base_key_t;

	private:
		fixed_point_t PROPERTY(base);
		std::vector<condition_weight_item_t> PROPERTY(condition_weight_items);
		scope_type_t PROPERTY(initial_scope);
		scope_type_t PROPERTY(this_scope);
		scope_type_t PROPERTY(from_scope);

		struct parse_scripts_visitor_t;

	public:
		ConditionalWeight(scope_type_t new_initial_scope, scope_type_t new_this_scope, scope_type_t new_from_scope);
		ConditionalWeight(ConditionalWeight&&) = default;

		static constexpr std::string_view base_key_to_string(base_key_t base_key) {
			switch (base_key) {
				case base_key_t::BASE: return "base";
				case base_key_t::FACTOR: return "factor";
				case base_key_t::MONTHS: return "months"; // TODO - add functionality for days or months or years
				default: return "INVALID BASE KEY";
			}
		}

		NodeTools::node_callback_t expect_conditional_weight(base_key_t base_key);

		bool parse_scripts(DefinitionManager const& definition_manager);
	};
}
