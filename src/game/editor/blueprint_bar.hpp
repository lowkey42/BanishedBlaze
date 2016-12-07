/** The sidebar to create entities from blueprints ***************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "editor_comp.hpp"

#include <core/asset/asset_manager.hpp>

#include <core/input/input_manager.hpp>

#include <core/graphic/texture.hpp>
#include <core/graphic/text.hpp>
#include <core/graphic/camera.hpp>
#include <core/graphic/texture_batch.hpp>

#include <core/ecs/ecs.hpp>

#include <core/utils/command.hpp>
#include <core/utils/messagebus.hpp>


namespace lux {
	namespace graphic {
		class Command_queue;
	}

namespace editor {

	struct Editor_conf;
	struct Blueprint_group;
	class Selection;

	class Blueprint_bar {
		public:
			Blueprint_bar(Engine&, util::Command_manager& commands, Selection& selection,
			              ecs::Entity_manager& entity_manager, asset::Asset_manager& assets,
			              input::Input_manager& input_manager,
			              graphic::Camera& camera_world, graphic::Camera_2d& camera_ui,
			              glm::vec2 offset);

			void draw(graphic::Command_queue& queue);
			void update(Time dt);
			auto handle_pointer(util::maybe<glm::vec2> mp1,
			                    util::maybe<glm::vec2> mp2) -> bool; //< true = mouse-input has been used

			auto is_in_delete_zone(util::maybe<glm::vec2> mp1) -> bool;

		private:
			static constexpr int idx_back = -1;
			static constexpr int idx_delete = -2;

			Engine&                             _engine;
			graphic::Camera&                   _camera_world;
			graphic::Camera_2d&                _camera_ui;
			glm::vec2                           _offset;
			util::Mailbox_collection            _mailbox;
			util::Command_manager&              _commands;
			Selection&                          _selection;
			ecs::Entity_manager&                _entity_manager;
			input::Input_manager&               _input_manager;
			graphic::Texture_ptr               _background;
			graphic::Texture_ptr               _back_button;
			graphic::Texture_ptr               _delete_button;
			std::shared_ptr<const Editor_conf>  _conf;
			util::maybe<const Blueprint_group&> _current_category = util::nothing();

			bool                                _mouse_pressed = false;
			glm::vec2                           _last_mouse_pos;
			util::maybe<int>                    _dragging = util::nothing();

			glm::vec2                           _tooltip_pos;
			Time                                _tooltip_delay_left{};
			graphic::Text_dynamic              _tooltip_text;
			mutable graphic::Texture_batch     _batch;

			void _spawn_new(int index, glm::vec2 pos);
			auto _get_index(glm::vec2 mouse_pos)const -> util::maybe<int>;
	};

}
}
