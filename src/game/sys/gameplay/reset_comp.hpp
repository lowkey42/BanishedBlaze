/** Marks entities, that are reset after all players died ********************
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
namespace gameplay {

	class Reset_comp : public ecs::Component<Reset_comp> {
		public:
			static constexpr const char* name() {return "Reset";}

			Reset_comp() = default;
			Reset_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}
	};

}
}
}
