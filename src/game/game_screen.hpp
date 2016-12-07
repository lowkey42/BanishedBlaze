/** The screen running the gameplay for a specific level *********************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "meta_system.hpp"

#include <core/graphic/camera.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/text.hpp>
#include <core/engine.hpp>
#include <core/utils/maybe.hpp>


namespace lux {

	class Game_screen : public Screen {
		public:
			Game_screen(Engine& game_engine, const std::string& level_id, bool add_to_highscore=false);
			~Game_screen()noexcept;

		protected:
			void _update(Time delta_time)override;
			void _draw()override;

			void _on_enter(util::maybe<Screen&> prev) override;
			void _on_leave(util::maybe<Screen&> next) override;

			auto _prev_screen_policy()const noexcept -> Prev_screen_policy override {
				return Prev_screen_policy::stack;
			}

		private:
			util::Mailbox_collection _mailbox;
			Meta_system _systems;
			bool _add_to_highscore;

			graphic::Text_dynamic _ui_text;
			graphic::Texture_ptr _hud_background;
			graphic::Texture_ptr _hud_timer_background;
			graphic::Texture_ptr _hud_light_icon;
			graphic::Texture_ptr _hud_dash_icon;
			graphic::Texture_ptr _hud_foreground;
			graphic::Shader_program _orb_shader;

			sys::gameplay::Player_tag_comp::Pool& _players;
			int _last_selected_idx=0;
			float _selection_movement = 0.f;

			graphic::Camera_2d _camera_ui;
			graphic::Command_queue _render_queue;

			std::string _current_level;
			std::string _music_aid;

			bool _fadeout = false;
			Time _fadeout_fadetimer {};
			util::maybe<Rgb> _org_sun_light = util::nothing();
			bool _reset_gameplay = false;

			Time _time_acc {0};

			auto _draw_orb(glm::vec2 pos, float scale, ecs::Entity_facet) -> graphic::Command;
			void _draw_orbs(sys::gameplay::Player_tag_comp::Pool::iterator selected,
			                bool left_side, int count, glm::vec2 hud_pos);
	};

}
