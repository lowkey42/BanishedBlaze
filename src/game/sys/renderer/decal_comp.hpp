/** A decal representation of an entity **************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/component.hpp>
#include <core/graphic/texture.hpp>


namespace lux {
namespace sys {
namespace renderer {

	class Decal_comp : public ecs::Component<Decal_comp> {
		public:
			static constexpr const char* name() {return "Decal";}
			friend void load_component(ecs::Deserializer& state, Decal_comp&);
			friend void save_component(ecs::Serializer& state, const Decal_comp&);

			Decal_comp() = default;
			Decal_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner) {}

		private:
			friend class Renderer_forward;

			graphic::Texture_ptr _texture;
			glm::vec2 _size;
	};

}
}
}
