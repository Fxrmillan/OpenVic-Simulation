#include "GoodInstance.hpp"

using namespace OpenVic;

GoodInstance::GoodInstance(GoodDefinition const& new_good_definition)
  : HasIdentifierAndColour { new_good_definition },
	buy_lock { std::make_unique<std::mutex>() },
	sell_lock { std::make_unique<std::mutex>() },
  	good_definition { new_good_definition },
	price { new_good_definition.get_base_price() },
	max_next_price {},
	min_next_price {},
	is_available { new_good_definition.get_is_available_from_start() },
	total_demand_yesterday { fixed_point_t::_0() },
	total_supply_yesterday { fixed_point_t::_0() },
	buy_up_to_orders {},
	market_sell_orders {}
	{ update_next_price_limits(); }

void GoodInstance::update_next_price_limits() {
	max_next_price = std::min(
		good_definition.get_base_price() * 5,
		price + fixed_point_t::_1() / fixed_point_t::_100()
	);
	min_next_price = std::max(
		good_definition.get_base_price() * 22 / fixed_point_t::_100(),
		price - fixed_point_t::_1() / fixed_point_t::_100()
	);
}

void GoodInstance::add_buy_up_to_order(const GoodBuyUpToOrder buy_up_to_order) {
	const std::lock_guard<std::mutex> lock {*buy_lock};
	buy_up_to_orders.push_back(buy_up_to_order);
}

void GoodInstance::add_market_sell_order(const GoodMarketSellOrder market_sell_order) {
	const std::lock_guard<std::mutex> lock {*sell_lock};
	market_sell_orders.push_back(market_sell_order);
}

void GoodInstance::execute_orders() {
	fixed_point_t demand_running_total = fixed_point_t::_0();
	for (GoodBuyUpToOrder const& buy_up_to_order : buy_up_to_orders) {
		demand_running_total += buy_up_to_order.get_max_quantity();
	}

	fixed_point_t supply_running_total = fixed_point_t::_0();
	for (GoodMarketSellOrder const& market_sell_order : market_sell_orders) {
		supply_running_total += market_sell_order.get_quantity();
	}
	
	fixed_point_t new_price;
	if (demand_running_total > supply_running_total) {
		new_price = max_next_price;
	} else if (demand_running_total < supply_running_total) {
		new_price = min_next_price;
	} else {
		new_price = price;
	}
	
	for (GoodBuyUpToOrder const& buy_up_to_order : buy_up_to_orders) {
		const fixed_point_t quantity_bought = buy_up_to_order.get_money_to_spend() / new_price;
		buy_up_to_order.get_after_trade()({
			quantity_bought,
			buy_up_to_order.get_money_to_spend() - quantity_bought * new_price
		});
	}

	for (GoodMarketSellOrder const& market_sell_order : market_sell_orders) {
		const fixed_point_t market_sell_quantity = market_sell_order.get_quantity();
		market_sell_order.get_after_trade()({
			market_sell_quantity,
			market_sell_quantity * new_price
		});
	}

	total_demand_yesterday = demand_running_total;
	total_supply_yesterday = supply_running_total;
	buy_up_to_orders.clear();
	market_sell_orders.clear();
	if (new_price != price) {
		update_next_price_limits();
	}
}

bool GoodInstanceManager::setup(GoodDefinitionManager const& good_definition_manager) {
	if (good_instances_are_locked()) {
		Logger::error("Cannot set up good instances - they are already locked!");
		return false;
	}

	good_instances.reserve(good_definition_manager.get_good_definition_count());

	bool ret = true;

	for (GoodDefinition const& good : good_definition_manager.get_good_definitions()) {
		ret &= good_instances.add_item({ good });
	}

	lock_good_instances();

	return ret;
}
