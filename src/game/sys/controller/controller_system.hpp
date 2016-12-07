/** System controlling the actions of all entites ****************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "input_controller_comp.hpp"
#include "ai_patrolling_comp.hpp"

#include <core/engine.hpp>
#include <core/units.hpp>
#include <core/ecs/ecs.hpp>


namespace lux {
namespace sys {
namespace physics {
	class Dynamic_body_comp;
	class Physics_system;
}
namespace controller {

	class Controller_system {
		public:
			Controller_system(Engine&, ecs::Entity_manager&, physics::Physics_system& physics_world);

			void update(Time);

			bool input_active();

			void block_input(Time t) {
				_input_block_remainder = t;
			}

			auto get_controlled() {
				return _active_controlled_entity;
			}
			void set_controlled(ecs::Entity_facet e) {
				_active_controlled_entity = e;
			}

		private:
			util::Mailbox_collection _mailbox;
			input::Input_manager& _input_manager;
			ecs::Entity_manager& _ecs;
			Input_controller_comp::Pool& _input_controllers;
			Ai_patrolling_comp::Pool& _ai_controllers;
			physics::Physics_system& _physics_world;

			bool _mouse_look = true;
			float _move_dir = 0.f;
			int _move_left = 0;
			int _move_right = 0;
			bool _jump = false;
			glm::vec2 _target_dir;
			bool _transform_pending = false;
			bool _transform = false;

			ecs::Entity_facet _active_controlled_entity;
			int _active_controlled_idx = 0;

			Time _input_block_remainder {};

			void _switch_controller(bool next);

			static void _move(Input_controller_comp& c, physics::Dynamic_body_comp& body, float dir, bool grounded, Time dt);
			static void _start_jump(Input_controller_comp& c, physics::Dynamic_body_comp& body, Time dt);
			static void _stop_jump(Input_controller_comp& c, physics::Dynamic_body_comp& body, Time dt);
	};

}
}
}
