#pragma once

#include "openvic-simulation/misc/Modifier.hpp"
#include "openvic-simulation/types/IdentifierRegistry.hpp"
#include "openvic-simulation/politics/Government.hpp"
#include "openvic-simulation/politics/Ideology.hpp"
#include <cstdint>
#include <unordered_map>

namespace OpenVic {
	struct RebelManager;

	struct RebelType : HasIdentifier {
		friend struct RebelManager;

		using government_map_t = std::unordered_map<GovernmentType const*, GovernmentType const*>;
		using icon_t = uint16_t;

		enum class area_t {
			NATION, NATION_RELIGION, NATION_CULTURE,
			CULTURE, CULTURE_GROUP, RELIGION, ALL
		};

		enum class defection_t {
			NONE, CULTURE, CULTURE_GROUP, RELIGION,
			IDEOLOGY, PAN_NATIONALIST, ANY
		};

		enum class independence_t {
			NONE, CULTURE, CULTURE_GROUP, RELIGION,
			COLONIAL, PAN_NATIONALIST, ANY
		};

	private:
		const icon_t PROPERTY(icon);
		const area_t PROPERTY(area);
		const bool PROPERTY_CUSTOM_PREFIX(break_alliance_on_win, will);
		government_map_t PROPERTY(desired_governments); //government
		const defection_t PROPERTY_CUSTOM_NAME(defection, get_defection_type);
		const independence_t PROPERTY_CUSTOM_NAME(independence, get_independence_type);
		const uint16_t PROPERTY(defect_delay);
		Ideology const* PROPERTY(ideology);
		const bool PROPERTY_CUSTOM_PREFIX(allow_all_cultures, will);
		const bool PROPERTY_CUSTOM_PREFIX(allow_all_culture_groups, will);
		const bool PROPERTY_CUSTOM_PREFIX(allow_all_religions, will);
		const bool PROPERTY_CUSTOM_PREFIX(allow_all_ideologies, will);
		const bool PROPERTY_CUSTOM_PREFIX(resilient, is);
		const bool PROPERTY_CUSTOM_PREFIX(reinforcing, is);
		const bool PROPERTY_CUSTOM_PREFIX(general, can_have);
		const bool PROPERTY_CUSTOM_PREFIX(smart, is);
		const bool PROPERTY_CUSTOM_NAME(unit_transfer, will_transfer_units);
		const fixed_point_t PROPERTY(occupation_mult);

		RebelType(
			std::string_view new_identifier, RebelType::icon_t icon, RebelType::area_t area, bool break_alliance_on_win,
			RebelType::government_map_t&& desired_governments, RebelType::defection_t defection,
			RebelType::independence_t independence, uint16_t defect_delay, Ideology const* ideology, bool allow_all_cultures,
			bool allow_all_culture_groups, bool allow_all_religions, bool allow_all_ideologies, bool resilient,
			bool reinforcing, bool general, bool smart, bool unit_transfer, fixed_point_t occupation_mult
		);

	public:
		RebelType(RebelType&&) = default;
	};

	struct RebelManager {
	private:
		IdentifierRegistry<RebelType> IDENTIFIER_REGISTRY(rebel_type);

	public:
		bool add_rebel_type(
			std::string_view new_identifier, RebelType::icon_t icon, RebelType::area_t area, bool break_alliance_on_win,
			RebelType::government_map_t&& desired_governments, RebelType::defection_t defection,
			RebelType::independence_t independence, uint16_t defect_delay, Ideology const* ideology, bool allow_all_cultures,
			bool allow_all_culture_groups, bool allow_all_religions, bool allow_all_ideologies, bool resilient,
			bool reinforcing, bool general, bool smart, bool unit_transfer, fixed_point_t occupation_mult
		);

		bool load_rebels_file(IdeologyManager const& ideology_manager, GovernmentTypeManager const& government_type_manager, ast::NodeCPtr root);
		bool generate_modifiers(ModifierManager& modifier_manager) const;
	};
}