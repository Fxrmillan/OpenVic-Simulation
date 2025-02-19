#include "ResourceGatheringOperation.hpp"

#include <vector>

#include "openvic-simulation/economy/production/Employee.hpp"
#include "openvic-simulation/economy/production/ProductionType.hpp"
#include "openvic-simulation/map/ProvinceInstance.hpp"
#include "openvic-simulation/map/State.hpp"
#include "openvic-simulation/modifier/ModifierEffectCache.hpp"
#include "openvic-simulation/pop/Pop.hpp"
#include "openvic-simulation/types/fixed_point/FixedPoint.hpp"
#include "openvic-simulation/utility/Logger.hpp"

using namespace OpenVic;

ResourceGatheringOperation::ResourceGatheringOperation(
	ProductionType const* new_production_type_nullable,
	fixed_point_t new_size_multiplier,
	fixed_point_t new_revenue_yesterday,
	fixed_point_t new_output_quantity_yesterday,
	fixed_point_t new_unsold_quantity_yesterday,
	std::vector<Employee>&& new_employees,
	decltype(employee_count_per_type_cache)::keys_t const& pop_type_keys
) : production_type_nullable { new_production_type_nullable },
	revenue_yesterday { new_revenue_yesterday },
	output_quantity_yesterday { new_output_quantity_yesterday },
	unsold_quantity_yesterday { new_unsold_quantity_yesterday },
	size_multiplier { new_size_multiplier },
	employees { std::move(new_employees) },
	max_employee_count_cache { 0 },
	total_employees_count_cache { 0 },
	total_paid_employees_count_cache { 0 },
	total_owner_income_cache { },
	total_employee_income_cache { },
	employee_count_per_type_cache { &pop_type_keys }
{ }

ResourceGatheringOperation::ResourceGatheringOperation(decltype(employee_count_per_type_cache)::keys_t const& pop_type_keys) : ResourceGatheringOperation {
	nullptr, fixed_point_t::_0(),
	fixed_point_t::_0(), fixed_point_t::_0(),
	fixed_point_t::_0(), {}, pop_type_keys
} {}

void ResourceGatheringOperation::initialise_for_new_game(ProvinceInstance& location, ModifierEffectCache const& modifier_effect_cache) {
	if (production_type_nullable == nullptr) {
		output_quantity_yesterday = 0;
		revenue_yesterday = 0;
		return;
	}

	ProductionType const& production_type = *production_type_nullable;
	const fixed_point_t size_modifier = calculate_size_modifier(location, modifier_effect_cache);
	const Pop::pop_size_t total_worker_count_in_province = update_size_and_return_total_worker_count(location, modifier_effect_cache, size_modifier);
	hire(location, total_worker_count_in_province);
	Pop::pop_size_t total_owner_count_in_state_cache = 0;
	std::vector<Pop*> owner_pops_cache {};
	output_quantity_yesterday = produce(location, owner_pops_cache, total_owner_count_in_state_cache, modifier_effect_cache, size_modifier);
	revenue_yesterday = output_quantity_yesterday * production_type.get_output_good().get_base_price(); //TODO sell on market
	pay_employees(location, revenue_yesterday, total_worker_count_in_province, owner_pops_cache, total_owner_count_in_state_cache);	
}

Pop::pop_size_t ResourceGatheringOperation::update_size_and_return_total_worker_count(
	ProvinceInstance& location,
	ModifierEffectCache const& modifier_effect_cache,
	const fixed_point_t size_modifier
) {
	if (production_type_nullable == nullptr) {
		size_multiplier = fixed_point_t::_0();
		max_employee_count_cache = fixed_point_t::_0();
		return fixed_point_t::_0();
	}
	
	Pop::pop_size_t total_worker_count_in_province = 0; //not counting equivalents
	ProductionType const& production_type = *production_type_nullable;
	std::vector<Job> const& jobs = production_type.get_jobs();
	//can't use pop_type_distribution as it is not filled correctly yet (possibly due to equivalent pop type conversion)
	for (Pop const& pop : location.get_pops()){
		PopType const* pop_type = pop.get_type();
		for(Job const& job : jobs) {
			if (job.get_pop_type() == pop_type) {
				total_worker_count_in_province += pop.get_size();
				break;
			}
		}
	}
	
	fixed_point_t base_size_modifier = fixed_point_t::_1();
	if (production_type.is_farm()) {
		base_size_modifier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_size_local());
	}
	if (production_type.is_mine()) {
		base_size_modifier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_size_local());
	}

	const fixed_point_t base_workforce_size = production_type.get_base_workforce_size();
	if (base_size_modifier == fixed_point_t::_0()) {
		size_multiplier = 0;
	} else {
		size_multiplier = ((total_worker_count_in_province / (base_size_modifier * base_workforce_size)).ceil() * fixed_point_t::_1_50()).floor();
	}
	max_employee_count_cache = (size_modifier * size_multiplier * base_workforce_size).floor();
	return total_worker_count_in_province;
}

fixed_point_t ResourceGatheringOperation::calculate_size_modifier(ProvinceInstance const& location, ModifierEffectCache const& modifier_effect_cache) const {
	if (production_type_nullable == nullptr) {
		return fixed_point_t::_1();
	}
	
	ProductionType const& production_type = *production_type_nullable;

	fixed_point_t size_modifier = fixed_point_t::_1();
	if (production_type.is_farm()) {
		size_modifier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_size_global())
			+ location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_size_local());
	}
	if (production_type.is_mine()) {
		size_modifier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_size_global())
			+ location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_size_local());
	}
	auto const& good_effects = modifier_effect_cache.get_good_effects()[production_type.get_output_good()];
	size_modifier += location.get_modifier_effect_value_nullcheck(good_effects.get_rgo_size());
	return size_modifier > fixed_point_t::_0() ? size_modifier : fixed_point_t::_0();
}

void ResourceGatheringOperation::hire(ProvinceInstance& location, Pop::pop_size_t available_worker_count) {
	total_employees_count_cache = 0;
	total_paid_employees_count_cache=0;
	if (production_type_nullable == nullptr) {
		employees.clear();
		employee_count_per_type_cache.fill(fixed_point_t::_0());
		return;
	}

	ProductionType const& production_type = *production_type_nullable;
	if (max_employee_count_cache <= 0) { return; }
	if (available_worker_count <= 0) { return; }

	fixed_point_t proportion_to_hire;
	if (max_employee_count_cache >= available_worker_count) {
		//hire everyone
		proportion_to_hire = fixed_point_t::_1();
	} else {
		//hire all pops proportionally
		const fixed_point_t max_worker_count_real = max_employee_count_cache, available_worker_count_real = available_worker_count;
		proportion_to_hire = max_worker_count_real / available_worker_count_real;
	}
	
	std::vector<Job> const& jobs = production_type.get_jobs();
	for (Pop& pop : location.get_mutable_pops()){
		PopType const& pop_type = *pop.get_type();
		for(Job const& job : jobs) {
			if (job.get_pop_type() == &pop_type) {
				const Pop::pop_size_t pop_size_to_hire = static_cast<Pop::pop_size_t>((proportion_to_hire * pop.get_size()).floor());
				employee_count_per_type_cache[pop_type] += pop_size_to_hire;
				employees.emplace_back(pop, pop_size_to_hire);
				total_employees_count_cache += pop_size_to_hire;
				if (!pop_type.get_is_slave()) {
					total_paid_employees_count_cache += pop_size_to_hire;
				}
				break;
			}
		}
	}
}

fixed_point_t ResourceGatheringOperation::produce(
	ProvinceInstance& location,
	std::vector<Pop*>& owner_pops_cache,
	Pop::pop_size_t& total_owner_count_in_state_cache,
	ModifierEffectCache const& modifier_effect_cache,
	const fixed_point_t size_modifier
) {
	if (size_modifier == fixed_point_t::_0()){
		return fixed_point_t::_0();
	}

	total_owner_count_in_state_cache = 0;
	owner_pops_cache = {};
	if (production_type_nullable == nullptr || max_employee_count_cache <= 0) {
		return fixed_point_t::_0();
	}
	
	ProductionType const& production_type = *production_type_nullable;
	fixed_point_t throughput_multiplier = fixed_point_t::_1();
	fixed_point_t output_multilpier = fixed_point_t::_1();

	std::optional<Job> const& owner = production_type.get_owner();
	if (owner.has_value()) {
		Job const& owner_job = owner.value();
		PopType const* owner_job_pop_type_nullable = owner_job.get_pop_type();
		if (owner_job_pop_type_nullable == nullptr) {
			Logger::error("Owner job for ", production_type.get_identifier(), " has nullptr as pop_type.");
			return fixed_point_t::_0();
		}
		PopType const& owner_pop_type = *owner_job_pop_type_nullable;
		State const* state_nullable = location.get_state();
		if (state_nullable == nullptr) {
			Logger::error("Province ", location.get_identifier(), " has no state.");
			return fixed_point_t::_0();
		}
		State const& state = *state_nullable;
		Pop::pop_size_t state_population = 0; //state.get_total_population() is not filled yet
		std::vector<ProvinceInstance*> const& provinces_in_state = state.get_provinces();
		for (ProvinceInstance* const province_nullable : provinces_in_state) {
			if (province_nullable == nullptr) {
				Logger::error("State ", state.get_identifier(), " has nullptr in provinces.");
				return fixed_point_t::_0();
			}
			ProvinceInstance& province = *province_nullable;
			for (Pop& pop : province.get_mutable_pops()){
				state_population += pop.get_size();
				if (&owner_pop_type == pop.get_type()) {
					owner_pops_cache.push_back(&pop);
					total_owner_count_in_state_cache += pop.get_size();
				}
			}
		}

		if (total_owner_count_in_state_cache > 0) {
			switch (owner_job.get_effect_type()) {
				case Job::effect_t::OUTPUT:
					output_multilpier += owner_job.get_effect_multiplier() * total_owner_count_in_state_cache / state_population;
					break;
				case Job::effect_t::THROUGHPUT:
					throughput_multiplier += owner_job.get_effect_multiplier() * total_owner_count_in_state_cache / state_population;
					break;
				default:
					Logger::error("Invalid job effect in RGO ",production_type.get_identifier());
					break;
			}
		}
	}

	throughput_multiplier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_rgo_throughput())
		+location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_local_rgo_throughput());
	output_multilpier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_rgo_output())
		+location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_local_rgo_output());

	if (production_type.is_farm()) {
		throughput_multiplier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_throughput_global());
		output_multilpier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_output_global())
			+ location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_farm_rgo_output_local());
	}
	if (production_type.is_mine()) {
		throughput_multiplier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_throughput_global());
		output_multilpier += location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_output_global())
			+ location.get_modifier_effect_value_nullcheck(modifier_effect_cache.get_mine_rgo_output_local());
	}
	auto const& good_effects = modifier_effect_cache.get_good_effects()[production_type.get_output_good()];
	throughput_multiplier += location.get_modifier_effect_value_nullcheck(good_effects.get_rgo_goods_throughput());
	output_multilpier += location.get_modifier_effect_value_nullcheck(good_effects.get_rgo_goods_output());

	fixed_point_t throughput_from_workers = fixed_point_t::_0();
	fixed_point_t output_from_workers = fixed_point_t::_1();
	for (PopType const& pop_type : *employee_count_per_type_cache.get_keys()) {
		const Pop::pop_size_t employees_of_type = employee_count_per_type_cache[pop_type];
		
		for(Job const& job : production_type.get_jobs()) {
			if (job.get_pop_type() != &pop_type) {
				continue;
			}

			fixed_point_t const effect_multiplier = job.get_effect_multiplier();
			fixed_point_t relative_to_workforce = fixed_point_t::parse(employees_of_type) / fixed_point_t::parse(max_employee_count_cache);
			fixed_point_t const amount = job.get_amount();
			if (effect_multiplier != fixed_point_t::_1() && relative_to_workforce > amount) {
				relative_to_workforce = amount;
			}
			switch (job.get_effect_type()) {
				case Job::effect_t::OUTPUT:
					output_from_workers += effect_multiplier * relative_to_workforce;
					break;
				case Job::effect_t::THROUGHPUT:
					throughput_from_workers += effect_multiplier * relative_to_workforce;
					break;
				default:
					Logger::error("Invalid job effect in RGO ",production_type.get_identifier());
					break;
			}
		}
	}
	
	//if province is overseas multiply by (1 + overseas penalty)

	return production_type.get_base_output_quantity()
		* size_modifier * size_multiplier
		* throughput_multiplier * throughput_from_workers
		* output_multilpier * output_from_workers;
}

void ResourceGatheringOperation::pay_employees(
	ProvinceInstance& location,
	const fixed_point_t revenue,
	const Pop::pop_size_t total_worker_count_in_province,
	std::vector<Pop*>& owner_pops_cache,
	const Pop::pop_size_t total_owner_count_in_state_cache
) {
	total_owner_income_cache = 0;
	total_employee_income_cache = 0;
	if (production_type_nullable == nullptr || revenue <= 0 || total_worker_count_in_province <= 0) {
		if (revenue < 0) { Logger::error("Negative revenue for province ", location.get_identifier()); }
		if (total_worker_count_in_province < 0) { Logger::error("Negative total worker count for province ", location.get_identifier()); }
		return;
	}
	
	ProductionType const& production_type = *production_type_nullable;

	fixed_point_t revenue_left = revenue;
	if (total_owner_count_in_state_cache > 0) {
		Job const& owner_job = production_type.get_owner().value();
		PopType const* owner_job_pop_type_nullable = owner_job.get_pop_type();
		
		fixed_point_t owner_share = (fixed_point_t::_2() * total_owner_count_in_state_cache / total_worker_count_in_province);
		constexpr fixed_point_t upper_limit = fixed_point_t::_0_50();
		if (owner_share > upper_limit) {
			owner_share = upper_limit;
		}

		for(Pop* owner_pop_nullable : owner_pops_cache) {
			Pop& owner_pop = *owner_pop_nullable;
			const fixed_point_t income_for_this_pop = revenue_left * owner_share * owner_pop.get_size() / total_owner_count_in_state_cache;
			owner_pop.add_rgo_owner_income(income_for_this_pop);
			total_owner_income_cache += income_for_this_pop;
		}
		revenue_left *= (fixed_point_t::_1() - owner_share);
	}

	if (total_paid_employees_count_cache > 0) {
		for (Employee& employee : employees) {
			Pop& employee_pop = employee.pop;
			PopType const* employee_pop_type_nullable = employee_pop.get_type();
			if (employee_pop_type_nullable == nullptr) {
				Logger::error("employee has nullptr pop_type.");
				return;
			}
			PopType const& employee_pop_type = *employee_pop_type_nullable;
			if (employee_pop_type.get_is_slave()) {
				continue;
			}

			const Pop::pop_size_t employee_size = employee.get_size();
			const fixed_point_t income_for_this_pop = revenue_left * employee_size / total_paid_employees_count_cache;
			employee_pop.add_rgo_worker_income(income_for_this_pop);
			total_employee_income_cache += income_for_this_pop;
		}
	} else {
		//scenario slaves only
		//Money is removed from system in Victoria 2.
	}
}