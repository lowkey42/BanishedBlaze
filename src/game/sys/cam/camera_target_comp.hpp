/** A simple camera that follows its entity **********************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/graphic/camera.hpp>
#include <core/engine.hpp>
#include <core/units.hpp>
#include <core/ecs/component.hpp>


namespace lux {
namespace sys {
namespace cam {

	class Camera_system;

	class Camera_target_comp : public ecs::Component<Camera_target_comp> {
		public:
			static constexpr auto name() {return "Camera_target";}

			Camera_target_comp() = default;
			Camera_target_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner);

			void active(bool a) {_active = a;}
			auto active()const noexcept {return _active;}

		private:
			friend class Camera_system;

			bool _active = true;
	};

}
}
}
