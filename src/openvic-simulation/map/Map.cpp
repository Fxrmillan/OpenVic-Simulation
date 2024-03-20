#include "Map.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

#include "openvic-simulation/history/ProvinceHistory.hpp"
#include "openvic-simulation/types/Colour.hpp"
#include "openvic-simulation/types/OrderedContainers.hpp"
#include "openvic-simulation/utility/BMP.hpp"
#include "openvic-simulation/utility/Logger.hpp"

using namespace OpenVic;
using namespace OpenVic::NodeTools;
using namespace OpenVic::colour_literals;

Mapmode::Mapmode(
	std::string_view new_identifier, index_t new_index, colour_func_t new_colour_func
) : HasIdentifier { new_identifier }, index { new_index }, colour_func { new_colour_func } {
	assert(colour_func != nullptr);
}

const Mapmode Mapmode::ERROR_MAPMODE {
	"mapmode_error", 0, [](Map const& map, Province const& province) -> base_stripe_t {
		return { 0xFFFF0000_argb, colour_argb_t::null() };
	}
};

Mapmode::base_stripe_t Mapmode::get_base_stripe_colours(Map const& map, Province const& province) const {
	return colour_func ? colour_func(map, province) : colour_argb_t::null();
}

Map::Map()
  : width { 0 }, height { 0 }, max_provinces { Province::MAX_INDEX }, selected_province { nullptr },
	highest_province_population { 0 }, total_map_population { 0 } {}

bool Map::add_province(std::string_view identifier, colour_t colour) {
	if (provinces.size() >= max_provinces) {
		Logger::error(
			"The map's province list is full - maximum number of provinces is ", max_provinces, " (this can be at most ",
			Province::MAX_INDEX, ")"
		);
		return false;
	}
	if (identifier.empty()) {
		Logger::error("Invalid province identifier - empty!");
		return false;
	}
	if (!valid_basic_identifier(identifier)) {
		Logger::error(
			"Invalid province identifier: ", identifier, " (can only contain alphanumeric characters and underscores)"
		);
		return false;
	}
	if (colour.is_null()) {
		Logger::error("Invalid province colour for ", identifier, " - null! (", colour, ")");
		return false;
	}
	Province new_province { identifier, colour, static_cast<Province::index_t>(provinces.size() + 1) };
	const Province::index_t index = get_index_from_colour(colour);
	if (index != Province::NULL_INDEX) {
		Logger::error(
			"Duplicate province colours: ", get_province_by_index(index)->to_string(), " and ", new_province.to_string()
		);
		return false;
	}
	colour_index_map[new_province.get_colour()] = new_province.get_index();
	return provinces.add_item(std::move(new_province));
}

Province::distance_t Map::calculate_distance_between(Province const& from, Province const& to) const {
	const fvec2_t to_pos = to.get_unit_position();
	const fvec2_t from_pos = from.get_unit_position();

	const fixed_point_t min_x = std::min(
		(to_pos.x - from_pos.x).abs(),
		std::min(
			(to_pos.x - from_pos.x + width).abs(),
			(to_pos.x - from_pos.x - width).abs()
		)
	);

	return fvec2_t { min_x, to_pos.y - from_pos.y }.length_squared().sqrt();
}

using adjacency_t = Province::adjacency_t;

/* This is called for all adjacent pixel pairs and returns whether or not a new adjacency was add,
 * hence the lack of error messages in the false return cases. */
bool Map::add_standard_adjacency(Province& from, Province& to) const {
	if (from == to) {
		return false;
	}

	const bool from_needs_adjacency = !from.is_adjacent_to(&to);
	const bool to_needs_adjacency = !to.is_adjacent_to(&from);

	if (!from_needs_adjacency && !to_needs_adjacency) {
		return false;
	}

	const Province::distance_t distance = calculate_distance_between(from, to);

	using enum adjacency_t::type_t;

	/* Default land-to-land adjacency */
	adjacency_t::type_t type = LAND;
	if (from.is_water() != to.is_water()) {
		/* Land-to-water adjacency */
		type = COASTAL;

		/* Mark the land province as coastal */
		from.coastal = !from.is_water();
		to.coastal = !to.is_water();
	} else if (from.is_water()) {
		/* Water-to-water adjacency */
		type = WATER;
	}

	if (from_needs_adjacency) {
		from.adjacencies.emplace_back(&to, distance, type, nullptr, 0);
	}
	if (to_needs_adjacency) {
		to.adjacencies.emplace_back(&from, distance, type, nullptr, 0);
	}
	return true;
}

bool Map::add_special_adjacency(
	Province& from, Province& to, adjacency_t::type_t type, Province const* through, adjacency_t::data_t data
) const {
	if (from == to) {
		Logger::error("Trying to add ", adjacency_t::get_type_name(type), " adjacency from province ", from, " to itself!");
		return false;
	}

	using enum adjacency_t::type_t;

	/* Check end points */
	switch (type) {
	case LAND:
	case STRAIT:
		if (from.is_water() || to.is_water()) {
			Logger::error(adjacency_t::get_type_name(type), " adjacency from ", from, " to ", to, " has water endpoint(s)!");
			return false;
		}
		break;
	case WATER:
	case CANAL:
		if (!from.is_water() || !to.is_water()) {
			Logger::error(adjacency_t::get_type_name(type), " adjacency from ", from, " to ", to, " has land endpoint(s)!");
			return false;
		}
		break;
	case COASTAL:
		if (from.is_water() == to.is_water()) {
			Logger::error("Coastal adjacency from ", from, " to ", to, " has both land or water endpoints!");
			return false;
		}
		break;
	case IMPASSABLE:
		/* Impassable is valid for all combinations of land and water:
		 * - land-land = replace existing land adjacency with impassable adjacency (blue borders)
		 * - land-water = delete existing coastal adjacency, preventing armies and navies from moving between the provinces
		 * - water-water = delete existing water adjacency, preventing navies from moving between the provinces */
		break;
	default:
		Logger::error("Invalid adjacency type ", static_cast<uint32_t>(type));
		return false;
	}

	/* Check through province */
	if (type == STRAIT || type == CANAL) {
		const bool water_expected = type == STRAIT;
		if (through == nullptr || through->is_water() != water_expected) {
			Logger::error(
				adjacency_t::get_type_name(type), " adjacency from ", from, " to ", to, " has a ",
				(through == nullptr ? "null" : water_expected ? "land" : "water"), " through province ", through
			);
			return false;
		}
	} else if (through != nullptr) {
		Logger::warning(
			adjacency_t::get_type_name(type), " adjacency from ", from, " to ", to, " has a non-null through province ",
			through
		);
		through = nullptr;
	}

	/* Check canal data */
	if (data != adjacency_t::NO_CANAL && type != CANAL) {
		Logger::warning(
			adjacency_t::get_type_name(type), " adjacency from ", from, " to ", to, " has invalid data ",
			static_cast<uint32_t>(data)
		);
		data = adjacency_t::NO_CANAL;
	}

	const Province::distance_t distance = calculate_distance_between(from, to);

	const auto add_adjacency = [distance, type, through, data](Province& from, Province const& to) -> bool {
		const std::vector<adjacency_t>::iterator existing_adjacency = std::find_if(
			from.adjacencies.begin(), from.adjacencies.end(),
			[&to](adjacency_t const& adj) -> bool { return adj.get_to() == &to; }
		);
		if (existing_adjacency != from.adjacencies.end()) {
			if (type == existing_adjacency->get_type()) {
				Logger::warning(
					"Adjacency from ", from, " to ", to, " already has type ", adjacency_t::get_type_name(type), "!"
				);
				if (type != STRAIT && type != CANAL) {
					/* Straits and canals might change through or data, otherwise we can exit early */
					return true;
				}
			}
			if (type == IMPASSABLE) {
				if (existing_adjacency->get_type() == WATER || existing_adjacency->get_type() == COASTAL) {
					from.adjacencies.erase(existing_adjacency);
					return true;
				}
			} else {
				if (type != STRAIT && type != CANAL) {
					Logger::error(
						"Provinces ", from, " and ", to, " already have an existing ",
						adjacency_t::get_type_name(existing_adjacency->get_type()), " adjacency, cannot create a ",
						adjacency_t::get_type_name(type), " adjacency!"
					);
					return false;
				}
				if (type != existing_adjacency->get_type() && existing_adjacency->get_type() != (type == CANAL ? WATER : LAND)) {
					Logger::error(
						"Cannot convert ", adjacency_t::get_type_name(existing_adjacency->get_type()), " adjacency from ", from,
						" to ", to, " to type ", adjacency_t::get_type_name(type), "!"
					);
					return false;
				}
			}
			*existing_adjacency = { &to, distance, type, through, data };
			return true;
		} else if (type == IMPASSABLE) {
			Logger::warning(
				"Provinces ", from, " and ", to, " do not have an existing adjacency to make impassable!"
			);
			return true;
		} else {
			from.adjacencies.emplace_back(&to, distance, type, through, data);
			return true;
		}
	};

	return add_adjacency(from, to) & add_adjacency(to, from);
}

bool Map::set_water_province(std::string_view identifier) {
	if (water_provinces.is_locked()) {
		Logger::error("The map's water provinces have already been locked!");
		return false;
	}
	Province* province = get_province_by_identifier(identifier);
	if (province == nullptr) {
		Logger::error("Unrecognised water province identifier: ", identifier);
		return false;
	}
	if (province->is_water()) {
		Logger::warning("Province ", identifier, " is already a water province!");
		return true;
	}
	if (!water_provinces.add_province(province)) {
		Logger::error("Failed to add province ", identifier, " to water province set!");
		return false;
	}
	province->water = true;
	return true;
}

bool Map::set_water_province_list(std::vector<std::string_view> const& list) {
	if (water_provinces.is_locked()) {
		Logger::error("The map's water provinces have already been locked!");
		return false;
	}
	bool ret = true;
	water_provinces.reserve_more(list.size());
	for (std::string_view const& identifier : list) {
		ret &= set_water_province(identifier);
	}
	lock_water_provinces();
	return ret;
}

void Map::lock_water_provinces() {
	water_provinces.lock();
	Logger::info("Locked water provinces after registering ", water_provinces.size());
}

bool Map::add_region(std::string_view identifier, Region::provinces_t const& provinces, colour_t colour) {
	if (identifier.empty()) {
		Logger::error("Invalid region identifier - empty!");
		return false;
	}
	if (provinces.empty()) {
		Logger::warning("No valid provinces in list for ", identifier);
		return true;
	}

	const bool meta = std::any_of(provinces.begin(), provinces.end(), std::bind_front(&Province::get_has_region));

	Region region { identifier, colour, meta };
	bool ret = region.add_provinces(provinces);
	region.lock();
	if (regions.add_item(std::move(region))) {
		if (!meta) {
			for (Province const* province : provinces) {
				remove_province_const(province)->has_region = true;
			}
		}
	} else {
		ret = false;
	}
	return ret;
}

Province::index_t Map::get_index_from_colour(colour_t colour) const {
	const colour_index_map_t::const_iterator it = colour_index_map.find(colour);
	if (it != colour_index_map.end()) {
		return it->second;
	}
	return Province::NULL_INDEX;
}

Province::index_t Map::get_province_index_at(size_t x, size_t y) const {
	if (x < width && y < height) {
		return province_shape_image[x + y * width].index;
	}
	return Province::NULL_INDEX;
}

bool Map::set_max_provinces(Province::index_t new_max_provinces) {
	if (new_max_provinces <= Province::NULL_INDEX) {
		Logger::error(
			"Trying to set max province count to an invalid value ", new_max_provinces, " (must be greater than ",
			Province::NULL_INDEX, ")"
		);
		return false;
	}
	if (!provinces.empty() || provinces.is_locked()) {
		Logger::error(
			"Trying to set max province count to ", new_max_provinces, " after provinces have already been added and/or locked"
		);
		return false;
	}
	max_provinces = new_max_provinces;
	return true;
}

void Map::set_selected_province(Province::index_t index) {
	if (index == Province::NULL_INDEX) {
		selected_province = nullptr;
	} else {
		selected_province = get_province_by_index(index);
		if (selected_province == nullptr) {
			Logger::error(
				"Trying to set selected province to an invalid index ", index, " (max index is ", get_province_count(), ")"
			);
		}
	}
}

Province* Map::get_selected_province() {
	return selected_province;
}

Province::index_t Map::get_selected_province_index() const {
	return selected_province != nullptr ? selected_province->get_index() : Province::NULL_INDEX;
}

bool Map::add_mapmode(std::string_view identifier, Mapmode::colour_func_t colour_func) {
	if (identifier.empty()) {
		Logger::error("Invalid mapmode identifier - empty!");
		return false;
	}
	if (colour_func == nullptr) {
		Logger::error("Mapmode colour function is null for identifier: ", identifier);
		return false;
	}
	return mapmodes.add_item({ identifier, mapmodes.size(), colour_func });
}

bool Map::generate_mapmode_colours(Mapmode::index_t index, uint8_t* target) const {
	if (target == nullptr) {
		Logger::error("Mapmode colour target pointer is null!");
		return false;
	}
	bool ret = true;
	Mapmode const* mapmode = mapmodes.get_item_by_index(index);
	if (mapmode == nullptr) {
		// Not an error if mapmodes haven't yet been loaded,
		// e.g. if we want to allocate the province colour
		// texture before mapmodes are loaded.
		if (!(mapmodes.empty() && index == 0)) {
			Logger::error("Invalid mapmode index: ", index);
			ret = false;
		}
		mapmode = &Mapmode::ERROR_MAPMODE;
	}
	// Skip past Province::NULL_INDEX
	for (size_t i = 0; i < sizeof(Mapmode::base_stripe_t); ++i) {
		*target++ = 0;
	}
	for (Province const& province : provinces.get_items()) {
		const Mapmode::base_stripe_t base_stripe = mapmode->get_base_stripe_colours(*this, province);
		colour_argb_t const& base_colour = base_stripe.base_colour;
		colour_argb_t const& stripe_colour = base_stripe.stripe_colour;

		*target++ = base_colour.red;
		*target++ = base_colour.green;
		*target++ = base_colour.blue;
		*target++ = base_colour.alpha;

		*target++ = stripe_colour.red;
		*target++ = stripe_colour.green;
		*target++ = stripe_colour.blue;
		*target++ = stripe_colour.alpha;
	}
	return ret;
}

void Map::update_highest_province_population() {
	highest_province_population = 0;
	for (Province const& province : provinces.get_items()) {
		highest_province_population = std::max(highest_province_population, province.get_total_population());
	}
}

void Map::update_total_map_population() {
	total_map_population = 0;
	for (Province const& province : provinces.get_items()) {
		total_map_population += province.get_total_population();
	}
}

bool Map::reset(BuildingTypeManager const& building_type_manager) {
	bool ret = true;
	for (Province& province : provinces.get_items()) {
		ret &= province.reset(building_type_manager);
	}
	return ret;
}

bool Map::apply_history_to_provinces(
	ProvinceHistoryManager const& history_manager, Date date, IdeologyManager const& ideology_manager,
	IssueManager const& issue_manager, Country const& country
) {
	bool ret = true;

	for (Province& province : provinces.get_items()) {
		if (!province.is_water()) {
			ProvinceHistoryMap const* history_map = history_manager.get_province_history(&province);

			if (history_map != nullptr) {
				ProvinceHistoryEntry const* pop_history_entry = nullptr;

				for (ProvinceHistoryEntry const* entry : history_map->get_entries_up_to(date)) {
					province.apply_history_to_province(entry);

					if (!entry->get_pops().empty()) {
						pop_history_entry = entry;
					}
				}

				if (pop_history_entry != nullptr) {
					province.add_pop_vec(pop_history_entry->get_pops());

					province.setup_pop_test_values(ideology_manager, issue_manager, country);
				}
			}
		}
	}

	return ret;
}

void Map::update_gamestate(Date today) {
	for (Province& province : provinces.get_items()) {
		province.update_gamestate(today);
	}
	update_highest_province_population();
	update_total_map_population();
}

void Map::tick(Date today) {
	for (Province& province : provinces.get_items()) {
		province.tick(today);
	}
}

using namespace ovdl::csv;

static bool _validate_province_definitions_header(LineObject const& header) {
	static const std::vector<std::string> standard_header { "province", "red", "green", "blue" };
	for (size_t i = 0; i < standard_header.size(); ++i) {
		const std::string_view val = header.get_value_for(i);
		if (i == 0 && val.empty()) {
			break;
		}
		if (val != standard_header[i]) {
			return false;
		}
	}
	return true;
}

static bool _parse_province_colour(colour_t& colour, std::array<std::string_view, 3> components) {
	bool ret = true;
	for (size_t i = 0; i < 3; ++i) {
		std::string_view& component = components[i];
		if (component.ends_with('.')) {
			component.remove_suffix(1);
		}
		bool successful = false;
		const uint64_t val = StringUtils::string_to_uint64(component, &successful, 10);
		if (successful && val <= colour_t::max_value) {
			colour[i] = val;
		} else {
			ret = false;
		}
	}
	return ret;
}

bool Map::load_province_definitions(std::vector<LineObject> const& lines) {
	if (lines.empty()) {
		Logger::error("No header or entries in province definition file!");
		return false;
	}

	{
		LineObject const& header = lines.front();
		if (!_validate_province_definitions_header(header)) {
			Logger::error(
				"Non-standard province definition file header - make sure this is not a province definition: ", header
			);
		}
	}

	if (lines.size() <= 1) {
		Logger::error("No entries in province definition file!");
		return false;
	}

	reserve_more_provinces(lines.size() - 1);

	bool ret = true;
	std::for_each(lines.begin() + 1, lines.end(), [this, &ret](LineObject const& line) -> void {
		const std::string_view identifier = line.get_value_for(0);
		if (!identifier.empty()) {
			colour_t colour = colour_t::null();
			if (!_parse_province_colour(colour, { line.get_value_for(1), line.get_value_for(2), line.get_value_for(3) })) {
				Logger::error("Error reading colour in province definition: ", line);
				ret = false;
			}
			ret &= add_province(identifier, colour);
		}
	});

	lock_provinces();

	return ret;
}

bool Map::load_province_positions(BuildingTypeManager const& building_type_manager, ast::NodeCPtr root) {
	return expect_province_dictionary([&building_type_manager](Province& province, ast::NodeCPtr node) -> bool {
		return province.load_positions(building_type_manager, node);
	})(root);
}

bool Map::load_region_colours(ast::NodeCPtr root, std::vector<colour_t>& colours) {
	return expect_dictionary_reserve_length(
		colours,
		[&colours](std::string_view key, ast::NodeCPtr value) -> bool {
			if (key != "color") {
				Logger::error("Invalid key in region colours: \"", key, "\"");
				return false;
			}
			return expect_colour(vector_callback(colours))(value);
	})(root);
}

bool Map::load_region_file(ast::NodeCPtr root, std::vector<colour_t> const& colours) {
	const bool ret = expect_dictionary_reserve_length(
		regions,
		[this, &colours](std::string_view region_identifier, ast::NodeCPtr region_node) -> bool {
			Region::provinces_t provinces;
			bool ret = expect_list_reserve_length(
				provinces, expect_province_identifier(vector_callback_pointer(provinces))
			)(region_node);
			ret &= add_region(region_identifier, provinces, colours[regions.size() % colours.size()]);
			return ret;
		}
	)(root);

	lock_regions();

	for (Region const& region : regions.get_items()) {
		if (!region.meta) {
			for (Province const* province : region.get_provinces()) {
				remove_province_const(province)->region = &region;
			}
		}
	}

	return ret;
}

static constexpr colour_t colour_at(uint8_t const* colour_data, int32_t idx) {
	/* colour_data is filled with BGR byte triplets - to get pixel idx as a
	 * single RGB value, multiply idx by 3 to get the index of the corresponding
	 * triplet, then combine the bytes in reverse order.
	 */
	idx *= 3;
	return { colour_data[idx + 2], colour_data[idx + 1], colour_data[idx] };
}

bool Map::load_map_images(fs::path const& province_path, fs::path const& terrain_path, bool detailed_errors) {
	if (!provinces.is_locked()) {
		Logger::error("Province index image cannot be generated until after provinces are locked!");
		return false;
	}
	if (!terrain_type_manager.terrain_type_mappings_are_locked()) {
		Logger::error("Province index image cannot be generated until after terrain type mappings are locked!");
		return false;
	}

	BMP province_bmp;
	if (!(province_bmp.open(province_path) && province_bmp.read_header() && province_bmp.read_pixel_data())) {
		Logger::error("Failed to read BMP for compatibility mode province image: ", province_path);
		return false;
	}
	static constexpr uint16_t expected_province_bpp = 24;
	if (province_bmp.get_bits_per_pixel() != expected_province_bpp) {
		Logger::error(
			"Invalid province BMP bits per pixel: ", province_bmp.get_bits_per_pixel(), " (expected ", expected_province_bpp,
			")"
		);
		return false;
	}

	BMP terrain_bmp;
	if (!(terrain_bmp.open(terrain_path) && terrain_bmp.read_header() && terrain_bmp.read_pixel_data())) {
		Logger::error("Failed to read BMP for compatibility mode terrain image: ", terrain_path);
		return false;
	}
	static constexpr uint16_t expected_terrain_bpp = 8;
	if (terrain_bmp.get_bits_per_pixel() != expected_terrain_bpp) {
		Logger::error(
			"Invalid terrain BMP bits per pixel: ", terrain_bmp.get_bits_per_pixel(), " (expected ", expected_terrain_bpp, ")"
		);
		return false;
	}

	if (province_bmp.get_width() != terrain_bmp.get_width() || province_bmp.get_height() != terrain_bmp.get_height()) {
		Logger::error(
			"Mismatched province and terrain BMP dims: ", province_bmp.get_width(), "x", province_bmp.get_height(), " vs ",
			terrain_bmp.get_width(), "x", terrain_bmp.get_height()
		);
		return false;
	}

	width = province_bmp.get_width();
	height = province_bmp.get_height();
	province_shape_image.resize(width * height);

	uint8_t const* province_data = province_bmp.get_pixel_data().data();
	uint8_t const* terrain_data = terrain_bmp.get_pixel_data().data();

	std::vector<fixed_point_map_t<TerrainType const*>> terrain_type_pixels_list(provinces.size());

	bool ret = true;
	ordered_set<colour_t> unrecognised_province_colours;

	std::vector<fixed_point_t> pixels_per_province(provinces.size());
	std::vector<fvec2_t> pixel_position_sum_per_province(provinces.size());

	for (int32_t y = 0; y < height; ++y) {
		for (int32_t x = 0; x < width; ++x) {
			const size_t pixel_index = x + y * width;
			const colour_t province_colour = colour_at(province_data, pixel_index);
			Province::index_t province_index = Province::NULL_INDEX;

			if (x > 0) {
				const size_t jdx = pixel_index - 1;
				if (colour_at(province_data, jdx) == province_colour) {
					province_index = province_shape_image[jdx].index;
					goto index_found;
				}
			}

			if (y > 0) {
				const size_t jdx = pixel_index - width;
				if (colour_at(province_data, jdx) == province_colour) {
					province_index = province_shape_image[jdx].index;
					goto index_found;
				}
			}

			province_index = get_index_from_colour(province_colour);

			if (province_index == Province::NULL_INDEX && !unrecognised_province_colours.contains(province_colour)) {
				unrecognised_province_colours.insert(province_colour);
				if (detailed_errors) {
					Logger::warning(
						"Unrecognised province colour ", province_colour, " at (", x, ", ", y, ")"
					);
				}
			}

		index_found:
			province_shape_image[pixel_index].index = province_index;

			if (province_index != Province::NULL_INDEX) {
				const Province::index_t array_index = province_index - 1;
				pixels_per_province[array_index]++;
				pixel_position_sum_per_province[array_index] += static_cast<fvec2_t>(ivec2_t { x, y });
			}

			const TerrainTypeMapping::index_t terrain = terrain_data[pixel_index];
			TerrainTypeMapping const* mapping = terrain_type_manager.get_terrain_type_mapping_for(terrain);
			if (mapping != nullptr) {
				if (province_index != Province::NULL_INDEX) {
					terrain_type_pixels_list[province_index - 1][&mapping->get_type()]++;
				}
				if (mapping->get_has_texture() && terrain < terrain_type_manager.get_terrain_texture_limit()) {
					province_shape_image[pixel_index].terrain = terrain + 1;
				} else {
					province_shape_image[pixel_index].terrain = 0;
				}
			} else {
				province_shape_image[pixel_index].terrain = 0;
			}
		}
	}

	if (!unrecognised_province_colours.empty()) {
		Logger::warning("Province image contains ", unrecognised_province_colours.size(), " unrecognised province colours");
	}

	size_t missing = 0;
	for (size_t array_index = 0; array_index < provinces.size(); ++array_index) {
		Province* province = provinces.get_item_by_index(array_index);

		fixed_point_map_t<TerrainType const*> const& terrain_type_pixels = terrain_type_pixels_list[array_index];
		const fixed_point_map_const_iterator_t<TerrainType const*> largest = get_largest_item(terrain_type_pixels);
		province->default_terrain_type = largest != terrain_type_pixels.end() ? largest->first : nullptr;

		const fixed_point_t pixel_count = pixels_per_province[array_index];
		province->on_map = pixel_count > 0;

		if (province->on_map) {
			province->positions.centre = pixel_position_sum_per_province[array_index] / pixel_count;
		} else {
			if (detailed_errors) {
				Logger::warning("Province missing from shape image: ", province->to_string());
			}
			missing++;
		}
	}
	if (missing > 0) {
		Logger::warning("Province image is missing ", missing, " province colours");
	}

	return ret;
}

/* REQUIREMENTS:
 * MAP-19, MAP-84
 */
bool Map::_generate_standard_province_adjacencies() {
	bool changed = false;

	const auto generate_adjacency = [this, &changed](Province* current, size_t x, size_t y) -> bool {
		Province* neighbour = get_province_by_index(province_shape_image[x + y * width].index);
		if (neighbour != nullptr) {
			return add_standard_adjacency(*current, *neighbour);
		}
		return false;
	};

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			Province* cur = get_province_by_index(province_shape_image[x + y * width].index);

			if (cur != nullptr) {
				changed |= generate_adjacency(cur, (x + 1) % width, y);
				if (y + 1 < height) {
					changed |= generate_adjacency(cur, x, y + 1);
				}
			}
		}
	}

	return changed;
}

bool Map::generate_and_load_province_adjacencies(std::vector<LineObject> const& additional_adjacencies) {
	bool ret = _generate_standard_province_adjacencies();
	if (!ret) {
		Logger::error("Failed to generate standard province adjacencies!");
	}
	/* Skip first line containing column headers */
	if (additional_adjacencies.size() <= 1) {
		Logger::error("No entries in province adjacencies file!");
		return false;
	}
	std::for_each(
		additional_adjacencies.begin() + 1, additional_adjacencies.end(), [this, &ret](LineObject const& adjacency) -> void {
			const std::string_view from_str = adjacency.get_value_for(0);
			if (from_str.empty() || from_str.front() == '#') {
				return;
			}
			Province* const from = get_province_by_identifier(from_str);
			if (from == nullptr) {
				Logger::error("Unrecognised adjacency from province identifier: \"", from_str, "\"");
				ret = false;
				return;
			}

			const std::string_view to_str = adjacency.get_value_for(1);
			Province* const to = get_province_by_identifier(to_str);
			if (to == nullptr) {
				Logger::error("Unrecognised adjacency to province identifier: \"", to_str, "\"");
				ret = false;
				return;
			}

			using enum adjacency_t::type_t;
			static const string_map_t<adjacency_t::type_t> type_map {
				{ "land", LAND }, { "sea", STRAIT }, { "impassable", IMPASSABLE }, { "canal", CANAL }
			};
			const std::string_view type_str = adjacency.get_value_for(2);
			const string_map_t<adjacency_t::type_t>::const_iterator it = type_map.find(type_str);
			if (it == type_map.end()) {
				Logger::error("Invalid adjacency type: \"", type_str, "\"");
				ret = false;
				return;
			}
			const adjacency_t::type_t type = it->second;

			Province const* const through = get_province_by_identifier(adjacency.get_value_for(3));

			const std::string_view data_str = adjacency.get_value_for(4);
			bool successful = false;
			const uint64_t data_uint = StringUtils::string_to_uint64(data_str, &successful);
			if (!successful || data_uint > std::numeric_limits<adjacency_t::data_t>::max()) {
				Logger::error("Invalid adjacency data: \"", data_str, "\"");
				ret = false;
				return;
			}
			const adjacency_t::data_t data = data_uint;

			ret &= add_special_adjacency(*from, *to, type, through, data);
		}
	);
	return ret;
}

bool Map::load_climate_file(ModifierManager const& modifier_manager, ast::NodeCPtr root) {
	bool ret = expect_dictionary_reserve_length(
		climates,
		[this, &modifier_manager](std::string_view identifier, ast::NodeCPtr node) -> bool {
			if (identifier.empty()) {
				Logger::error("Invalid climate identifier - empty!");
				return false;
			}

			bool ret = true;
			Climate* cur_climate = climates.get_item_by_identifier(identifier);
			if (cur_climate == nullptr) {
				ModifierValue values;
				ret &= modifier_manager.expect_modifier_value(move_variable_callback(values))(node);
				ret &= climates.add_item({ identifier, std::move(values) });
			} else {
				ret &= expect_list_reserve_length(*cur_climate, expect_province_identifier(
					[cur_climate, &identifier](Province& province) {
						if (province.climate != cur_climate) {
							cur_climate->add_province(&province);
							if (province.climate != nullptr) {
								Climate* old_climate = const_cast<Climate*>(province.climate);
								old_climate->remove_province(&province);
								Logger::warning(
									"Province with id ", province.get_identifier(),
									" found in multiple climates: ", identifier,
									" and ", old_climate->get_identifier()
								);
							}
							province.climate = cur_climate;
						} else {
							Logger::warning(
								"Province with id ", province.get_identifier(),
								" defined twice in climate ", identifier
							);
						}
						return true;
					}
				))(node);
			}
			return ret;
		}
	)(root);

	for (Climate& climate : climates.get_items()) {
		climate.lock();
	}

	lock_climates();

	return ret;
}

bool Map::load_continent_file(ModifierManager const& modifier_manager, ast::NodeCPtr root) {
	bool ret = expect_dictionary_reserve_length(
		continents,
		[this, &modifier_manager](std::string_view identifier, ast::NodeCPtr node) -> bool {
			if (identifier.empty()) {
				Logger::error("Invalid continent identifier - empty!");
				return false;
			}

			ModifierValue values;
			ProvinceSetModifier::provinces_t prov_list;
			bool ret = modifier_manager.expect_modifier_value_and_keys(move_variable_callback(values),
				"provinces", ONE_EXACTLY, expect_list_reserve_length(prov_list, expect_province_identifier(
					[&prov_list](Province const& province) -> bool {
						if (province.continent == nullptr) {
							prov_list.emplace_back(&province);
						} else {
							Logger::warning("Province ", province, " found in multiple continents");
						}
						return true;
					}
				))
			)(node);

			Continent continent = { identifier, std::move(values) };
			continent.add_provinces(prov_list);
			continent.lock();

			if (continents.add_item(std::move(continent))) {
				Continent const& moved_continent = continents.get_items().back();
				for (Province const* prov : moved_continent.get_provinces()) {
					remove_province_const(prov)->continent = &moved_continent;
				}
			} else {
				ret = false;
			}

			return ret;
		}
	)(root);

	lock_continents();

	return ret;
}
