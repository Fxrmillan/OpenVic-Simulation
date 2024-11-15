#pragma once

#include "openvic-simulation/economy/production/ProductionType.hpp"
#include "openvic-simulation/economy/trading/MarketInstance.hpp"
#include "openvic-simulation/modifier/ModifierEffectCache.hpp"
#include "openvic-simulation/pop/Pop.hpp"
#include "openvic-simulation/types/fixed_point/FixedPoint.hpp"
#include "openvic-simulation/utility/Getters.hpp"

namespace OpenVic {
	struct ArtisanalProducer {
	private:
		MarketInstance& market_instance;
		ModifierEffectCache const& modifier_effect_cache;
		Pop& pop;
		GoodDefinition::good_definition_map_t stockpile;
		ProductionType const& PROPERTY(production_type);
		fixed_point_t PROPERTY(current_production);

	public:
		ArtisanalProducer(
			MarketInstance& new_market_instance,
			ModifierEffectCache const& new_modifier_effect_cache,
			Pop& new_pop,
			GoodDefinition::good_definition_map_t&& new_stockpile,
			ProductionType const& new_production_type,
			fixed_point_t new_current_production
		);
		void artisan_tick();
	};
}
