#pragma once

#include <functional>

#include "Region.hpp"

namespace OpenVic {

	struct Mapmode : HasIdentifier {
		friend struct Map;

		using colour_func_t = std::function<colour_t(Map const&, Province const&)>;
		using index_t = size_t;

	private:
		const index_t index;
		const colour_func_t colour_func;

		Mapmode(index_t new_index, ovstring const& new_identifier, colour_func_t new_colour_func);

	public:
		static const Mapmode ERROR_MAPMODE;

		index_t get_index() const;
		colour_t get_colour(Map const& map, Province const& province) const;
	};

	struct GoodManager;

	/* REQUIREMENTS:
	 * MAP-4
	 */
	struct Map {
		using terrain_t = uint8_t;
		using terrain_variant_map_t = std::map<colour_t, terrain_t>;

#pragma pack(push, 1)
		struct shape_pixel_t {
			index_t index;
			terrain_t terrain;
		};
#pragma pack(pop)
	private:
		using colour_index_map_t = std::map<colour_t, index_t>;

		IdentifierRegistry<Province> provinces;
		IdentifierRegistry<Region> regions;
		IdentifierRegistry<Mapmode> mapmodes;
		bool water_provinces_locked = false;
		size_t water_province_count = 0;

		size_t width = 0, height = 0;
		std::vector<shape_pixel_t> province_shape_image;
		colour_index_map_t colour_index_map;
		index_t selected_province = NULL_INDEX;

		index_t get_index_from_colour(colour_t colour) const;

	public:
		Map();

		return_t add_province(ovstring const& identifier, colour_t colour);
		void lock_provinces();
		return_t set_water_province(ovstring const& identifier);
		void lock_water_provinces();
		return_t add_region(ovstring const& identifier, std::vector<ovstring> const& province_identifiers);
		void lock_regions();

		size_t get_province_count() const;
		Province* get_province_by_index(index_t index);
		Province const* get_province_by_index(index_t index) const;
		Province* get_province_by_identifier(ovstring const& identifier);
		Province const* get_province_by_identifier(ovstring const& identifier) const;
		index_t get_province_index_at(size_t x, size_t y) const;
		void set_selected_province(index_t index);
		index_t get_selected_province_index() const;
		Province const* get_selected_province() const;

		Region* get_region_by_identifier(ovstring const& identifier);
		Region const* get_region_by_identifier(ovstring const& identifier) const;

		return_t generate_province_shape_image(size_t new_width, size_t new_height, uint8_t const* colour_data,
			uint8_t const* terrain_data, terrain_variant_map_t const& terrain_variant_map);
		size_t get_width() const;
		size_t get_height() const;
		std::vector<shape_pixel_t> const& get_province_shape_image() const;

		return_t add_mapmode(ovstring const& identifier, Mapmode::colour_func_t colour_func);
		void lock_mapmodes();
		size_t get_mapmode_count() const;
		Mapmode const* get_mapmode_by_index(Mapmode::index_t index) const;
		Mapmode const* get_mapmode_by_identifier(ovstring const& identifier) const;
		static constexpr size_t MAPMODE_COLOUR_SIZE = 4;
		return_t generate_mapmode_colours(Mapmode::index_t index, uint8_t* target) const;

		return_t setup(GoodManager const& good_manager, BuildingManager const& building_manager);

		void update_state(Date const& today);
		void tick(Date const& today);
	};
}
