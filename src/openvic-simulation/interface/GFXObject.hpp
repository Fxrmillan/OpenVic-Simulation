#pragma once

#include "openvic-simulation/interface/LoadBase.hpp"
#include <openvic-simulation/types/unlabelledVec.hpp>

namespace OpenVic::GFX {

	class Object : public Named<> {
	protected:
		Object() = default;

	public:
		Object(Object&&) = default;
		virtual ~Object() = default;

		OV_DETAIL_GET_BASE_TYPE(Object)
		OV_DETAIL_GET_TYPE

		static NodeTools::node_callback_t expect_objects(
			NodeTools::length_callback_t length_callback, NodeTools::callback_t<std::unique_ptr<Object>&&> callback
		);
	};

	class Actor final : public Object {
		friend std::unique_ptr<Actor> std::make_unique<Actor>();

	public:
		class Attachment {
			friend class Actor;

		public:
			using attach_id_t = uint32_t;

		private:
			std::string PROPERTY(actor_name);
			std::string PROPERTY(attach_node);
			attach_id_t PROPERTY(attach_id);

			Attachment(std::string_view new_actor_name, std::string_view new_attach_node, attach_id_t new_attach_id);

		public:
			Attachment(Attachment&&) = default;
		};

		class Animation {
			friend class Actor;

			std::string PROPERTY(file);
			fixed_point_t PROPERTY(scroll_time);

			Animation(std::string_view new_file, fixed_point_t new_scroll_time);

		public:
			Animation(Animation&&) = default;
		};

	private:
		fixed_point_t PROPERTY(scale);
		std::string PROPERTY(model_file);
		std::optional<Animation> PROPERTY(idle_animation);
		std::optional<Animation> PROPERTY(move_animation);
		std::optional<Animation> PROPERTY(attack_animation);
		std::vector<Attachment> PROPERTY(attachments);

		bool _set_animation(std::string_view name, std::string_view file, fixed_point_t scroll_time);

	protected:
		Actor();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		Actor(Actor&&) = default;
		virtual ~Actor() = default;

		OV_DETAIL_GET_TYPE
	};

	/* arrows.gfx */
	class ArrowType final : public Object {
		friend std::unique_ptr<ArrowType> std::make_unique<ArrowType>();
	
	private:
		//Named<> already handles the name property
		fixed_point_t PROPERTY(size);
		std::string PROPERTY(texture_file);
		std::string PROPERTY(body_texture_file);
		colour_t PROPERTY(back_colour);
		colour_t PROPERTY(progress_colour);

		fixed_point_t PROPERTY(end_at); //how should float be repd? >> fixed_point handles it
		fixed_point_t PROPERTY(height);
		uint64_t PROPERTY(arrow_type);
		fixed_point_t PROPERTY(heading); //also float

		std::string PROPERTY(effect_file);

	protected:
		ArrowType();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		ArrowType(ArrowType&&) = default;
		virtual ~ArrowType() = default;

		OV_DETAIL_GET_TYPE
	};

	/* battlearrow.gfx */
	class BattleArrow final : public Object {
		friend std::unique_ptr<BattleArrow> std::make_unique<BattleArrow>();
	
	private:
		//Named<> already handles the name property
		//TODO verify the texture places
		std::string PROPERTY(texture_file);
		std::string PROPERTY(back_texture_file);

		fixed_point_t PROPERTY(start);
		fixed_point_t PROPERTY(stop);
		fixed_point_t PROPERTY(x);
		fixed_point_t PROPERTY(y);

		std::string PROPERTY(font);
		fixed_point_t PROPERTY(scale);
		bool PROPERTY(no_fade);
		fixed_point_t PROPERTY(texture_loop);

	protected:
		BattleArrow();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		BattleArrow(BattleArrow&&) = default;
		virtual ~BattleArrow() = default;

		OV_DETAIL_GET_TYPE
	};

	class MapInfo final : public Object {
		friend std::unique_ptr<MapInfo> std::make_unique<MapInfo>();
	
	private:
		std::string PROPERTY(texture_file);
		fixed_point_t PROPERTY(scale);

	protected:
		MapInfo();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		MapInfo(MapInfo&&) = default;
		virtual ~MapInfo() = default;

		OV_DETAIL_GET_TYPE
	};

	/* mapitems.gfx */
	class Projection final : public Object {
		friend std::unique_ptr<Projection> std::make_unique<Projection>();
	
	private:
		//Named<> already handles the name property
		std::string PROPERTY(texture_file);
		//TODO: should size, pulseSpeed, duration, fadeout be ints or fixed points? assume fixed_point_t to start
		fixed_point_t PROPERTY(size);
		fixed_point_t PROPERTY(spin);
		bool PROPERTY(pulsating);
		fixed_point_t PROPERTY(pulse_lowest);
		fixed_point_t PROPERTY(pulse_speed);
		bool PROPERTY(additative);
		fixed_point_t PROPERTY(expanding);
		std::optional<fixed_point_t> PROPERTY(duration);
		std::optional<fixed_point_t> PROPERTY(fadeout);

	protected:
		Projection();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		Projection(Projection&&) = default;
		virtual ~Projection() = default;

		OV_DETAIL_GET_TYPE
	};

	class Billboard final : public Object {
		friend std::unique_ptr<Billboard> std::make_unique<Billboard>();

	public:

	private:
		std::string PROPERTY(texture_file);
		fixed_point_t PROPERTY(scale);
		int64_t PROPERTY(no_of_frames);
		int64_t PROPERTY(font_size); //TODO: is this fixed point?
		V2Vector3 PROPERTY(offset);
		std::string PROPERTY(font);

	protected:
		Billboard();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		Billboard(Billboard&&) = default;
		virtual ~Billboard() = default;

		OV_DETAIL_GET_TYPE
	};


	class UnitStatsBillboard final : public Object {
		friend std::unique_ptr<UnitStatsBillboard> std::make_unique<UnitStatsBillboard>();

	private:
		//Named<> already handles the name property
		std::string PROPERTY(texture_file);
		std::string PROPERTY(effect_file);
		std::string PROPERTY(mask_file);
		fixed_point_t PROPERTY(scale);
		int64_t PROPERTY(no_of_frames);
		int64_t PROPERTY(font_size); //TODO: is this fixed point?
		std::string PROPERTY(font);

	protected:
		UnitStatsBillboard();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		UnitStatsBillboard(UnitStatsBillboard&&) = default;
		virtual ~UnitStatsBillboard() = default;

		OV_DETAIL_GET_TYPE
	};

	class ProgressBar3d final : public Object {
		friend std::unique_ptr<ProgressBar3d> std::make_unique<ProgressBar3d>();

	private:
		//Named<> already handles the name property
		//TODO: are there any other properties which are unused
		colour_t PROPERTY(back_colour);
		colour_t PROPERTY(progress_colour);
		ivec2_t PROPERTY(size);
		std::string PROPERTY(effect_file);

	protected:
		ProgressBar3d();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		ProgressBar3d(ProgressBar3d&&) = default;
		virtual ~ProgressBar3d() = default;

		OV_DETAIL_GET_TYPE
	};

	/* Core.gfx */
	class AnimatedMapText final : public Object {
		friend std::unique_ptr<AnimatedMapText> std::make_unique<AnimatedMapText>();

		//TODO: This format_t enum is the same as in GFXSprite, should this all be extracted
		//to another datatype file?
		enum class format_t {
			left, centre, right, justified
		};

	public:
		struct TextBlock {
			friend class AnimatedMapText;
			//format_t taken from GUI's AlignedElement, this is overall very similar

		
		private:
			std::string PROPERTY(text);
			colour_t PROPERTY(colour);
			std::string PROPERTY(font);

			fvec2_t PROPERTY(text_position);
			fvec2_t PROPERTY(size);
			//expect_fvec2(assign_variable_callback(position)),

			format_t PROPERTY(format);

			TextBlock();

		public:
			TextBlock(TextBlock&&) = default;
		};

	private:
		fixed_point_t PROPERTY(speed);
		fixed_point_t PROPERTY(scale);
		V2Vector3 PROPERTY(position);
		TextBlock PROPERTY(textblock);

	protected:
		AnimatedMapText();

		bool _fill_key_map(NodeTools::case_insensitive_key_map_t& key_map) override;

	public:
		AnimatedMapText(AnimatedMapText&&) = default;
		virtual ~AnimatedMapText() = default;

		OV_DETAIL_GET_TYPE
	};

}