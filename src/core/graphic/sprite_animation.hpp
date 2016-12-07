/**************************************************************************\
 * Animation data for textures and sprites                                *
 *                                               ___                      *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2016 Florian Oetke                                       *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include "texture.hpp"
#include "sprite_batch.hpp"
#include "material.hpp"

#include "../asset/asset_manager.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <vector>
#include <stdexcept>


namespace lux {
namespace util {
	class Message_bus;
}

namespace graphic {

	using Animation_clip_id = util::Str_id;

	class Sprite_animation_state;

	struct Animation_event {
		util::Str_id name;
		void* owner;
	};
	struct Sprite_animation_Clip;

	class Sprite_animation_set {
		public:
			Sprite_animation_set(asset::istream&);
			auto operator=(Sprite_animation_set&&)noexcept -> Sprite_animation_set&;
			~Sprite_animation_set();

		private:
			friend class Sprite_animation_state;

			struct PImpl;

			std::unique_ptr<PImpl> _impl;

			void _register_inst(Sprite_animation_state&)const;
			void _unregister_inst(Sprite_animation_state&)const;
	};

	using Sprite_animation_set_ptr = asset::Ptr<Sprite_animation_set>;


	class Sprite_animation_state {
		public:
			Sprite_animation_state(void* owner=nullptr, Sprite_animation_set_ptr data={},
			                       Animation_clip_id initial_clip=""_strid);
			Sprite_animation_state(Sprite_animation_state&&)noexcept;
			auto operator=(Sprite_animation_state&&)noexcept -> Sprite_animation_state&;
			~Sprite_animation_state();

			auto owner()noexcept {return _owner;}

			void update(Time dt, util::Message_bus&);

			auto uv_rect()const -> glm::vec4;
			auto material()const -> const Material&;

			auto animation_set()const {return _animation_set;}
			void animation_set(Sprite_animation_set_ptr set, Animation_clip_id clip=""_strid);

			auto get_clip()const noexcept {return _curr_clip_id;}
			void set_clip(Animation_clip_id id);
			auto set_clip_if(Animation_clip_id id, Animation_clip_id expected) {
				if(_curr_clip_id==expected) {
					set_clip(id);
					return true;
				}
				return false;
			}
			void set_next_clip(Animation_clip_id id) {
				_queued_clip_id = id;
			}

			void speed(float factor) {
				_speed_factor = factor;
			}

			auto playing()const noexcept {return _playing;}

			void reset();

		private:
			void* _owner;
			Sprite_animation_set_ptr _animation_set;
			Animation_clip_id _curr_clip_id;
			util::maybe<Sprite_animation_Clip&> _curr_clip;
			util::maybe<Animation_clip_id> _queued_clip_id = util::nothing();
			int_fast16_t _frame = 0;
			Time _runtime {};
			float _speed_factor = 1.f;
			bool _playing = true;
	};

} /* namespace renderer */


namespace asset {
	template<>
	struct Loader<graphic::Sprite_animation_set> {
		static auto load(istream in) {
			return std::make_shared<graphic::Sprite_animation_set>(in);
		}

		static void store(ostream, const graphic::Sprite_animation_set&) {
			FAIL("NOT IMPLEMENTED!");
		}
	};
}

}
