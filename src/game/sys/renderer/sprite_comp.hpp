/** A sprite representation of an entity *************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/component.hpp>
#include <core/graphic/material.hpp>
#include <core/graphic/sprite_animation.hpp>


namespace lux {
namespace sys {
namespace renderer {

	inline auto rgb_to_hue(Rgb c) {
		auto min = std::min({c.r, c.g, c.b});
		auto max = std::max({c.r, c.g, c.b});
		auto h = 0.f;

		if(max==c.r)      h = (c.g-c.b)/(max-min);
		else if(max==c.g) h = 2.0 + (c.b-c.r)/(max-min);
		else if(max==c.b) h = 4.0 + (c.r-c.g)/(max-min);

		h*=60.f;
		if(h<0)
			h+=360.f;
		return Angle::from_degrees(h);
	}

	class Sprite_comp : public ecs::Component<Sprite_comp> {
		public:
			static constexpr const char* name() {return "Sprite";}
			friend void load_component(ecs::Deserializer& state, Sprite_comp&);
			friend void save_component(ecs::Serializer& state, const Sprite_comp&);

			Sprite_comp() = default;
			Sprite_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
			            graphic::Material_ptr material = {}) :
				Component(manager, owner), _material(material) {}

			auto size()const noexcept {return _size;}
			void size(glm::vec2 size) {_size = size;}

			void hue_change_replacement(Angle a) {
				_hue_change_replacement = a;
			}

		private:
			friend class Renderer_forward;

			graphic::Material_ptr _material;
			glm::vec2 _size;
			bool _shadowcaster = true;
			float _shadow_receiver = 1.f;
			float _decals_intensity = 0.f;
			glm::vec2 _decals_position;
			bool _decals_position_set = false;
			bool _decals_sticky = false; //< attach decals to intial position
			Angle _hue_change_target {0};
			Angle _hue_change_replacement {0};
	};

	class Anim_sprite_comp : public ecs::Component<Anim_sprite_comp> {
		public:
			static constexpr const char* name() {return "Anim_sprite";}
			friend void load_component(ecs::Deserializer& state, Anim_sprite_comp&);
			friend void save_component(ecs::Serializer& state, const Anim_sprite_comp&);

			Anim_sprite_comp() = default;
			Anim_sprite_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner);

			auto size()const noexcept {return _size;}
			void size(glm::vec2 size) {_size = size;}

			auto& state()      noexcept {return _anim_state;}
			auto& state()const noexcept {return _anim_state;}

			void play(graphic::Animation_clip_id id, float speed=1.f);
			void play_if(graphic::Animation_clip_id expected,
			             graphic::Animation_clip_id id, float speed=1.f);
			void play_next(graphic::Animation_clip_id id);
			auto playing()const noexcept -> util::maybe<graphic::Animation_clip_id>;

			void hue_change_replacement(Angle a) {
				_hue_change_replacement = a;
			}

		private:
			friend class Renderer_forward;

			graphic::Sprite_animation_state _anim_state;
			glm::vec2 _size;
			bool _shadowcaster = true;
			float _shadow_receiver = 1.f;
			float _decals_intensity = 0.f;
			glm::vec2 _decals_position;
			bool _decals_position_set = false;
			bool _decals_sticky = false; //< attach decals to intial position
			Angle _hue_change_target {0};
			Angle _hue_change_replacement {0};
	};

}
}
}
