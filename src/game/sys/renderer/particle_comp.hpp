/** A particle emmiter attached to an entity *********************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/component.hpp>
#include <core/graphic/particles.hpp>


namespace lux {
namespace sys {
namespace renderer {

	class Particle_comp : public ecs::Component<Particle_comp> {
		public:
			static constexpr const char* name() {return "Particle";}
			friend void load_component(ecs::Deserializer& state, Particle_comp&);
			friend void save_component(ecs::Serializer& state, const Particle_comp&);

			Particle_comp() = default;
			Particle_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
			              graphic::Particle_emitter_ptr e = {})
			    : Component(manager, owner), _emitters{{e}} {}

			void add(graphic::Particle_type_id id);
			void remove(graphic::Particle_type_id id);

			void hue_change(Angle a) {
				_hue_change = a;
			}

		private:
			friend class Renderer_forward;

			glm::vec3 _offset;
			Angle _hue_change;

			std::array<graphic::Particle_type_id, 3> _add_queue;
			std::array<graphic::Particle_emitter_ptr, 3> _emitters;
	};

}
}
}
