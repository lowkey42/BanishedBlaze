/** An entity that is directly controlled by an HID **************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/engine.hpp>
#include <core/units.hpp>
#include <core/ecs/component.hpp>


namespace lux {
namespace sys {
namespace controller {

	class Controller_system;

	class Input_controller_comp : public ecs::Component<Input_controller_comp> {
		public:
			static constexpr auto name() {return "Input_controller";}
			friend void load_component(ecs::Deserializer& state, Input_controller_comp&);
			friend void save_component(ecs::Serializer& state, const Input_controller_comp&);

			Input_controller_comp() = default;
			Input_controller_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner);

		private:
			friend class Controller_system;

			float _air_velocity = 1.f;
			float _ground_velocity = 1.f;
			float _acceleration_time = 0.2f;

			float _jump_velocity = 1.f;
			float _jump_cooldown = 0.25f;

			float _last_velocity = 0.f;
			Time _moving_time{0};
			Time _air_time{0};
			Time _jump_cooldown_timer{0};
			Time _air_dash_timer {0};
	};

}
}
}
