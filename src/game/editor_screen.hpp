/** The ingame level editor **************************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "level.hpp"
#include "meta_system.hpp"

#include "editor/blueprint_bar.hpp"
#include "editor/selection.hpp"
#include "editor/level_settings.hpp"
#include "editor/menu_bar.hpp"

#include <core/graphic/camera.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/vertex_object.hpp>
#include <core/graphic/primitives.hpp>
#include <core/graphic/text.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/utils/command.hpp>
#include <core/engine.hpp>


namespace lux {

	class Editor_screen : public Screen {
		public:
			Editor_screen(Engine& game_engine, const std::string& level_id);
			~Editor_screen()noexcept = default;

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
			util::Command_manager _commands;
			input::Input_manager& _input_manager;

			Meta_system _systems;

			graphic::Camera_2d _camera_menu;
			graphic::Camera_sidescroller _camera_world;

			graphic::Text_dynamic _cmd_text;
			graphic::Texture_ptr  _cmd_background;

			mutable graphic::Texture_batch _batch;
			graphic::Command_queue         _render_queue;

			editor::Selection _selection;
			editor::Blueprint_bar _blueprints;
			editor::Menu_bar _menu;
			editor::Level_settings _settings;

			util::maybe<std::string> _clipboard;
			util::maybe<glm::vec2> _last_pointer_pos;
			bool _cam_mouse_active = false;
			glm::vec2 _cam_speed;
			util::maybe<float> _ambient_light_override = util::nothing();

			util::Command_marker _last_saved;
			Level_info _level_metadata;

			util::maybe<std::string> _load_requested = util::nothing();
			bool _load_ready = false;

			auto _handle_pointer_cam(util::maybe<glm::vec2> mp1, util::maybe<glm::vec2> mp2) -> bool;
			void _load_next_level(int dir);
			bool _load_next_level_allowed();
	};

}
