/** The main menu ************************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/graphic/camera.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/text.hpp>
#include <core/engine.hpp>
#include <core/utils/maybe.hpp>


namespace lux {

	class Main_menu_screen : public Screen {
		public:
			Main_menu_screen(Engine& engine);
			~Main_menu_screen()noexcept = default;

		protected:
			void _update(Time delta_time)override;
			void _draw()override;

			void _on_enter(util::maybe<Screen&> prev) override;
			void _on_leave(util::maybe<Screen&> next) override;

			auto _prev_screen_policy()const noexcept -> Prev_screen_policy override {
				return Prev_screen_policy::discard;
			}

		private:
			util::Mailbox_collection _mailbox;

			graphic::Camera_2d _camera_ui;
			graphic::Command_queue _render_queue;

			int _active_menu_entry = -1;
	};

}
