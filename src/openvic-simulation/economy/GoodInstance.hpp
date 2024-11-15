#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include "openvic-simulation/economy/GoodDefinition.hpp"
#include "openvic-simulation/economy/trading/BuyUpToOrder.hpp"
#include "openvic-simulation/economy/trading/MarketSellOrder.hpp"
#include "openvic-simulation/types/fixed_point/FixedPoint.hpp"
#include "openvic-simulation/types/HasIdentifier.hpp"
#include "openvic-simulation/types/IdentifierRegistry.hpp"
#include "openvic-simulation/utility/Getters.hpp"

namespace OpenVic {
	struct GoodInstanceManager;

	struct GoodInstance : HasIdentifierAndColour {
		friend struct GoodInstanceManager;

	private:
		std::unique_ptr<std::mutex> buy_lock;
		std::unique_ptr<std::mutex> sell_lock;
		GoodDefinition const& PROPERTY(good_definition);
		fixed_point_t PROPERTY(price);
		fixed_point_t PROPERTY(max_next_price);
		fixed_point_t PROPERTY(min_next_price);
		bool PROPERTY(is_available);
		fixed_point_t PROPERTY(total_demand_yesterday);
		fixed_point_t PROPERTY(total_supply_yesterday);
		std::deque<GoodBuyUpToOrder> buy_up_to_orders;
		std::deque<GoodMarketSellOrder> market_sell_orders;
		
		GoodInstance(GoodDefinition const& new_good_definition);

		void update_next_price_limits();
	public:
		GoodInstance(GoodInstance&&) = default;

		//thread safe
		void add_buy_up_to_order(const GoodBuyUpToOrder buy_up_to_order);
		void add_market_sell_order(const GoodMarketSellOrder market_sell_order);

		//not thread safe
		void execute_orders();
	};

	struct GoodInstanceManager {
	private:
		IdentifierRegistry<GoodInstance> IDENTIFIER_REGISTRY(good_instance);

	public:
		IDENTIFIER_REGISTRY_NON_CONST_ACCESSORS(good_instance);
		bool setup(GoodDefinitionManager const& good_definition_manager);
	};
}
