/** A sprite representation of an entity *************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/component.hpp>
#include <core/graphic/smart_texture.hpp>

namespace lux {
namespace sys {
namespace renderer {

	class Terrain_data_comp : public ecs::Component<Terrain_data_comp> {
		public:
			static constexpr const char* name() {return "Terrain_data";}
			friend void load_component(ecs::Deserializer& state, Terrain_data_comp&);
			friend void save_component(ecs::Serializer& state, const Terrain_data_comp&);

			Terrain_data_comp() = default;
			Terrain_data_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
			    : Component(manager, owner){}
	};


	class Terrain_comp : public ecs::Component<Terrain_comp> {
		public:
			static constexpr const char* name() {return "Terrain";}
			friend void load_component(ecs::Deserializer& state, Terrain_comp&);
			friend void save_component(ecs::Serializer& state, const Terrain_comp&);

			Terrain_comp() = default;
			Terrain_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
			             graphic::Material_ptr material = {});

			auto& smart_texture()noexcept {return _smart_texture;}
			auto& smart_texture()const noexcept {return _smart_texture;}

		private:
			friend class Renderer_forward;

			graphic::Smart_texture _smart_texture;
	};

}
}
}
