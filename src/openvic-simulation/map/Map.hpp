#pragma once

#include <filesystem>
#include <functional>

#include <openvic-dataloader/csv/LineObject.hpp>

#include "openvic-simulation/map/Region.hpp"
#include "openvic-simulation/map/State.hpp"
#include "openvic-simulation/map/TerrainType.hpp"
#include "openvic-simulation/types/Colour.hpp"

namespace OpenVic {
	namespace fs = std::filesystem;

	struct Mapmode : HasIdentifier {
		friend struct Map;

		/* Bottom 32 bits are the base colour, top 32 are the stripe colour, both in ARGB format with the alpha channels
		 * controlling interpolation with the terrain colour (0 = all terrain, 255 = all corresponding RGB) */
		struct base_stripe_t {
			colour_argb_t base_colour;
			colour_argb_t stripe_colour;
			constexpr base_stripe_t(colour_argb_t base, colour_argb_t stripe)
				: base_colour { base }, stripe_colour { stripe } {}
			constexpr base_stripe_t(colour_argb_t both) : base_stripe_t { both, both } {}
		};
		using colour_func_t = std::function<base_stripe_t(Map const&, Province const&)>;
		using index_t = size_t;

	private:
		const index_t PROPERTY(index);
		const colour_func_t colour_func;

		Mapmode(std::string_view new_identifier, index_t new_index, colour_func_t new_colour_func);

	public:
		static const Mapmode ERROR_MAPMODE;

		Mapmode(Mapmode&&) = default;

		base_stripe_t get_base_stripe_colours(Map const& map, Province const& province) const;
	};

	struct GoodManager;
	struct ProvinceHistoryManager;

	/* REQUIREMENTS:
	 * MAP-4
	 */
	struct Map {

#pragma pack(push, 1)
		/* Used to represent tightly packed 3-byte integer pixel information. */
		struct shape_pixel_t {
			Province::index_t index;
			TerrainTypeMapping::index_t terrain;
		};
#pragma pack(pop)
	private:
		using colour_index_map_t = std::map<colour_t, Province::index_t>;

		IdentifierRegistry<Province> IDENTIFIER_REGISTRY_CUSTOM_INDEX_OFFSET(province, 1);
		IdentifierRegistry<Region> IDENTIFIER_REGISTRY(region);
		IdentifierRegistry<Mapmode> IDENTIFIER_REGISTRY(mapmode);
		ProvinceSet water_provinces;
		TerrainTypeManager PROPERTY_REF(terrain_type_manager);

		int32_t PROPERTY(width);
		int32_t PROPERTY(height);
		std::vector<shape_pixel_t> PROPERTY(province_shape_image);
		colour_index_map_t colour_index_map;

		Province::index_t PROPERTY(max_provinces);
		Province::index_t PROPERTY(selected_province_index);
		Pop::pop_size_t PROPERTY(highest_province_population)
		Pop::pop_size_t PROPERTY(total_map_population);

		Province::index_t get_index_from_colour(colour_t colour) const;
		bool _generate_standard_province_adjacencies();

		StateManager PROPERTY_REF(state_manager);

	public:
		Map();

		bool add_province(std::string_view identifier, colour_t colour);
		IDENTIFIER_REGISTRY_NON_CONST_ACCESSORS_CUSTOM_INDEX_OFFSET(province, 1);

		bool set_water_province(std::string_view identifier);
		bool set_water_province_list(std::vector<std::string_view> const& list);
		void lock_water_provinces();

		Province::index_t get_province_index_at(size_t x, size_t y) const;
		bool set_max_provinces(Province::index_t new_max_provinces);
		void set_selected_province(Province::index_t index);
		Province const* get_selected_province() const;

		bool add_region(std::string_view identifier, std::vector<std::string_view> const& province_identifiers);
		IDENTIFIER_REGISTRY_NON_CONST_ACCESSORS(region)

		bool add_mapmode(std::string_view identifier, Mapmode::colour_func_t colour_func);

		/* The mapmode colour image contains of a list of base colours and stripe colours. Each colour is four bytes
		 * in RGBA format, with the alpha value being used to interpolate with the terrain colour, so A = 0 is fully terrain
		 * and A = 255 is fully the RGB colour packaged with A. The base and stripe colours for each province are packed
		 * together adjacently, so each province's entry is 8 bytes long. The list contains Province::MAX_INDEX + 1 entries,
		 * that is the maximum allowed number of provinces plus one for the index-zero "null province". */
		bool generate_mapmode_colours(Mapmode::index_t index, uint8_t* target) const;

		bool reset(BuildingTypeManager const& building_type_manager);
		bool apply_history_to_provinces(ProvinceHistoryManager const& history_manager, Date date);

		void update_highest_province_population();
		void update_total_map_population();

		void update_state(Date today);
		void tick(Date today);

		bool load_province_definitions(std::vector<ovdl::csv::LineObject> const& lines);
		/* Must be loaded after adjacencies so we know what provinces are coastal, and so can have a port */
		bool load_province_positions(BuildingTypeManager const& building_type_manager, ast::NodeCPtr root);
		bool load_region_file(ast::NodeCPtr root);
		bool load_map_images(fs::path const& province_path, fs::path const& terrain_path, bool detailed_errors);
		bool generate_and_load_province_adjacencies(std::vector<ovdl::csv::LineObject> const& additional_adjacencies);
	};
}
