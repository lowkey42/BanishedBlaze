/** editor specific state ****************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/graphic/texture.hpp>

#include <core/ecs/component.hpp>
#include <core/units.hpp>


namespace lux {
namespace editor {

	class Blueprint_bar;

	class Editor_comp : public ecs::Component<Editor_comp> {
		public:
			static constexpr auto name() {return "Editor";}
			friend void load_component(ecs::Deserializer&, Editor_comp&);
			friend void save_component(ecs::Serializer&,   const Editor_comp&);

			Editor_comp() = default;
			Editor_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)noexcept
			  : Component(manager, owner) {}

			auto bounds()const noexcept {return _bounds;}
			auto bounds(Position bounds) {_bounds = bounds;}

		private:
			friend class Blueprint_bar;

			Position _bounds;
	};


}
}
