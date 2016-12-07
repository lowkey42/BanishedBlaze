/** Handles the editor selection (drawing, input and actions) ****************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "editor_comp.hpp"

#include <core/ecs/ecs.hpp>

#include <core/graphic/camera.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/vertex_object.hpp>
#include <core/graphic/texture_batch.hpp>

#include <core/utils/command.hpp>
#include <core/units.hpp>
#include <core/engine.hpp>


namespace lux {
namespace editor {

	class Selection {
		public:
			Selection(Engine& engine, ecs::Entity_manager& entity_manager,
			          graphic::Camera& world_cam, util::Command_manager&);

			void select(ecs::Entity_facet e) {_selected_entity = std::move(e);}
			auto selection() {return _selected_entity;}

			void draw(graphic::Command_queue& queue, graphic::Camera&);
			void update();
			auto handle_pointer(util::maybe<glm::vec2> mp1,
			                    util::maybe<glm::vec2> mp2) -> bool; //< true = mouse-input has been used
			auto active()const -> bool {return _current_action!=Action_type::inactive && _current_action!=Action_type::none;}

			auto copy_content()const -> std::string;

			void snap_to_grid(bool s)noexcept {_snap_to_grid = s;}
			auto snap_to_grid()const noexcept {return _snap_to_grid;}

		private:
			enum class Action_type {
				inactive, none, move, scale, rotate, layer, mod_form
			};

			util::Mailbox_collection _mailbox;
			graphic::Camera& _world_cam;
			util::Command_manager& _commands;
			input::Input_manager& _input_manager;
			ecs::Entity_manager& _ecs;
			Editor_comp::Pool& _editor_comps;

			graphic::Texture_batch _batch;

			graphic::Texture_ptr _icon_layer;
			graphic::Texture_ptr _icon_move;
			graphic::Texture_ptr _icon_rotate;
			graphic::Texture_ptr _icon_scale;

			ecs::Entity_facet _selected_entity;
			bool      _curr_copy = false;
			bool      _curr_copy_created = false;
			glm::vec3 _curr_entity_position;
			Angle     _curr_entity_rotation;
			float     _curr_entity_scale = 1.f;
			glm::vec2 _curr_point_position;

			glm::vec3 _prev_entity_position;
			Angle     _prev_entity_rotation;
			float     _prev_entity_scale = 1.f;
			glm::vec2 _prev_point_position;
			bool      _point_created=false;

			util::maybe<glm::vec2> _last_primary_pointer_pos;
			util::maybe<glm::vec2> _last_secondary_pointer_pos;
			glm::vec2              _mouse_pressed_pos;

			Action_type _current_action = Action_type::inactive;
			int _current_shape_index = 0;
			bool _snap_to_grid = true;
			bool _copy_dragging = false;


			// all coordinates in screen space
			auto _handle_multitouch(glm::vec2 mp1, glm::vec2 mp2) -> bool;
			auto _handle_singletouch(glm::vec2 mp1_prev, glm::vec2 mp2_curr) -> bool;
			void _on_mouse_pressed(glm::vec2 mp);
			void _on_mouse_released(glm::vec2 mp);

			void _change_selection(glm::vec2 point_screenspace, bool cycle=true);
			void _move(glm::vec2 offset);
			void _move_layer(float offset);
			void _rotate(Angle offset);
			void _rotate(glm::vec2 pivot, Angle offset);
			void _scale(float factor);
			void _scale(glm::vec2 pivot, float factor);
			auto _insert_point(int prev, glm::vec2 position) -> int;
			void _move_point(glm::vec2 offset);
			void _update_entity_transform();
	};

}
}
