/** Screen that is shown after each level, to add an entry to the highscore **
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "game_engine.hpp"
#include "highscore_manager.hpp"

#include <core/gui/gui.hpp>
#include <core/graphic/camera.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/vertex_object.hpp>
#include <core/graphic/primitives.hpp>
#include <core/graphic/text.hpp>
#include <core/graphic/command_queue.hpp>


namespace lux {

	class Highscore_add_screen : public Screen {
		public:
			Highscore_add_screen(Engine& engine, std::string level, Time time);
			~Highscore_add_screen()noexcept = default;

		protected:
			void _update(Time delta_time)override;
			void _draw()override;

			void _on_enter(util::maybe<Screen&> prev) override;
			void _on_leave(util::maybe<Screen&> next) override;

			auto _prev_screen_policy()const noexcept -> Prev_screen_policy override {
				return Prev_screen_policy::draw;
			}

		private:
			util::Mailbox_collection _mailbox;
			Highscore_manager& _highscores;

			graphic::Camera_2d _camera;

			graphic::Text_dynamic _debug_Text;

			graphic::Command_queue _render_queue;

			Highscore_list_ptr _highscore_list;

			const std::string _level_id;
			const util::maybe<std::string> _next_level_id;
			const Time _time;
			gui::Text_edit _player_name;
			std::string _player_name_str;
	};

}
