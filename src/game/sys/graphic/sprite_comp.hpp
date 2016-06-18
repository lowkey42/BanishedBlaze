/** A sprite representation of an entity *************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/ecs.hpp>
#include <core/renderer/material.hpp>
#include <core/renderer/sprite_animation.hpp>


namespace lux {
namespace sys {
namespace graphic {

	class Sprite_comp : public ecs::Component<Sprite_comp> {
		public:
			static constexpr const char* name() {return "Sprite";}
			void load(sf2::JsonDeserializer& state,
			          asset::Asset_manager& asset_mgr)override;
			void save(sf2::JsonSerializer& state)const override;

			Sprite_comp(ecs::Entity& owner, renderer::Material_ptr material = {}) :
				Component(owner), _material(material) {}

			auto size()const noexcept {return _size;}
			void size(glm::vec2 size) {_size = size;}

		private:
			friend class Graphic_system;

			renderer::Material_ptr _material;
			glm::vec2 _size;
			bool _shadowcaster = true;
			float _decals_intensity = 0.f;
			Angle _hue_change_target {0};
			Angle _hue_change_replacement {0};
	};

	class Anim_sprite_comp : public ecs::Component<Anim_sprite_comp> {
		public:
			static constexpr const char* name() {return "Anim_sprite";}
			void load(sf2::JsonDeserializer& state,
			          asset::Asset_manager& asset_mgr)override;
			void save(sf2::JsonSerializer& state)const override;

			Anim_sprite_comp(ecs::Entity& owner);

			auto size()const noexcept {return _size;}
			void size(glm::vec2 size) {_size = size;}

			auto& state()      noexcept {return _anim_state;}
			auto& state()const noexcept {return _anim_state;}

			void play(renderer::Animation_clip_id id, float speed=1.f);
			void play_if(renderer::Animation_clip_id expected,
			             renderer::Animation_clip_id id, float speed=1.f);
			void play_next(renderer::Animation_clip_id id);
			auto playing()const noexcept -> util::maybe<renderer::Animation_clip_id>;

		private:
			friend class Graphic_system;

			renderer::Sprite_animation_state _anim_state;
			glm::vec2 _size;
			bool _shadowcaster = true;
			float _decals_intensity = 0.f;
			Angle _hue_change_target {0};
			Angle _hue_change_replacement {0};
	};

}
}
}
