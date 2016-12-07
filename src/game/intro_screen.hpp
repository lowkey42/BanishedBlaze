/** The intro screen that is shown when launching the game *******************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/engine.hpp>
#include <core/graphic/camera.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/vertex_object.hpp>
#include <core/graphic/primitives.hpp>
#include <core/graphic/text.hpp>
#include <core/graphic/command_queue.hpp>

namespace lux {

	class Intro_screen : public Screen {
		public:
			Intro_screen(Engine& game_engine);
			~Intro_screen()noexcept = default;

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

			graphic::Camera_2d _camera;

			graphic::Text_dynamic _debug_Text;

			graphic::Command_queue _render_queue;
	};

}
