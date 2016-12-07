/** The hub screen for a single world ****************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "level.hpp"

#include <core/graphic/camera.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/text.hpp>
#include <core/engine.hpp>
#include <core/utils/maybe.hpp>


namespace lux {

	class World_map_screen : public Screen {
		public:
			World_map_screen(Engine& game_engine, const std::string& level_pack_id);
			~World_map_screen()noexcept = default;

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

			graphic::Text_dynamic _ui_text;
			graphic::Camera_2d _camera_ui;
			graphic::Command_queue _render_queue;

			Level_pack_ptr _level_pack;
			int _current_level = 0;

			void _enter_nth_level(std::size_t idx);
	};

}
