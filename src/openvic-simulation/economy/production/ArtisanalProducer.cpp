#include "ArtisanalProducer.hpp"

#include "openvic-simulation/economy/GoodDefinition.hpp"
#include "openvic-simulation/economy/trading/BuyResult.hpp"
#include "openvic-simulation/economy/trading/SellResult.hpp"

using namespace OpenVic;

ArtisanalProducer::ArtisanalProducer(
	MarketInstance& new_market_instance,
	ModifierEffectCache const& new_modifier_effect_cache,
	Pop& new_pop,
	GoodDefinition::good_definition_map_t&& new_stockpile,
	ProductionType const& new_production_type,
	fixed_point_t new_current_production
) : market_instance { new_market_instance },
	modifier_effect_cache { new_modifier_effect_cache },
	pop { new_pop },
	stockpile { new_stockpile },
	production_type { new_production_type },
	current_production { new_current_production }
	{}

void ArtisanalProducer::artisan_tick() {
	GoodDefinition::good_definition_map_t goods_to_buy_and_max_price { };
	GoodDefinition::good_definition_map_t demand { };
	fixed_point_t inputs_bought_scalar = fixed_point_t::_1();
	if (production_type.get_input_goods().empty()) {
		inputs_bought_scalar = fixed_point_t::_1();
	} else {
		GoodInstanceManager const& good_instance_manager = *market_instance.get_good_instance_manager();
		for (auto const& [input_good_ptr, base_desired_quantity] : production_type.get_input_goods()) {
			const fixed_point_t desired_quantity = demand[input_good_ptr] = base_desired_quantity * pop.get_size() / production_type.get_base_workforce_size();
			inputs_bought_scalar = std::min(stockpile[input_good_ptr] / desired_quantity, inputs_bought_scalar);
			GoodInstance const& good = *good_instance_manager.get_good_instance_by_identifier(
				input_good_ptr->get_identifier()
			);
			goods_to_buy_and_max_price[input_good_ptr] = good.get_max_next_price();
		}

		if (inputs_bought_scalar > fixed_point_t::_0()) {
			for (auto const& [input_good_ptr, base_desired_quantity] : production_type.get_input_goods()) {
				const fixed_point_t desired_quantity = demand[input_good_ptr];
				stockpile[input_good_ptr] = std::max(
					fixed_point_t::_0(), 
					stockpile[input_good_ptr] - desired_quantity * inputs_bought_scalar
				);

				if (stockpile[input_good_ptr] >= desired_quantity) {
					goods_to_buy_and_max_price.erase(input_good_ptr);
				}
			}
		}
		
		const fixed_point_t total_cash_to_spend = pop.get_cash();
		if (total_cash_to_spend > 0 && !goods_to_buy_and_max_price.empty()) {
			fixed_point_t max_possible_satisfaction = fixed_point_t::_1();

			bool at_or_below_optimum = false;
			while (!at_or_below_optimum) {
				at_or_below_optimum = true;
				fixed_point_t total_demand_value = fixed_point_t::_0();
				fixed_point_t total_stockpile_value = fixed_point_t::_0();
				for (auto const& [input_good_ptr, max_price] : goods_to_buy_and_max_price) {
					total_demand_value += max_price * demand[input_good_ptr];
					total_stockpile_value += max_price * stockpile[input_good_ptr];
				}

				max_possible_satisfaction = std::min(
					fixed_point_t::_1(),
					(total_stockpile_value + total_cash_to_spend) / total_demand_value
				);

				for (auto const& [input_good_ptr, max_price] : goods_to_buy_and_max_price) {
					const fixed_point_t optimal_quantity = demand[input_good_ptr] * max_possible_satisfaction;
					if (stockpile[input_good_ptr] >= optimal_quantity) {
						goods_to_buy_and_max_price.erase(input_good_ptr);
						at_or_below_optimum = false;
					}
				}
			}

			for (auto const& [input_good_ptr, max_price] : goods_to_buy_and_max_price) {
				const fixed_point_t optimal_quantity = demand[input_good_ptr] * max_possible_satisfaction;
				const fixed_point_t money_to_spend = optimal_quantity * max_price;
				//TODO pop cash -= money_to_spend
				market_instance.place_buy_up_to_order({
					*input_good_ptr,
					optimal_quantity,
					money_to_spend,
					[this, input_good_ptr](const BuyResult buy_result) -> void {
						//TODO pop cash += buy_result.get_money_left()
						stockpile[input_good_ptr] += buy_result.get_quantity_bought();
					}
				});
			}
		}
	}

	current_production = production_type.get_base_output_quantity()
		* inputs_bought_scalar
		* pop.get_size() / production_type.get_base_workforce_size();

	GoodDefinition const& output_good = production_type.get_output_good();
	if (current_production > 0) {
		market_instance.place_market_sell_order({
			output_good,
			current_production,
			[this](const SellResult sell_result) -> void {
				//TODO add artisanal income to pop (part of https://github.com/OpenVicProject/OpenVic-Simulation/issues/225 )
			}
		});
	}
}