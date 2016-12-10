/** A simple point or spot light *********************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <core/units.hpp>
#include <core/ecs/component.hpp>


namespace lux {
namespace sys {
namespace renderer {

	struct Light_comp : public ecs::Component<Light_comp> {
		public:
			static constexpr const char* name() {return "Light";}
			friend void load_component(ecs::Deserializer& state, Light_comp&);
			friend void save_component(ecs::Serializer& state, const Light_comp&);

			Light_comp() = default;
			Light_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner);

			auto color(Rgb color)noexcept {_color=color;}
			auto color()const noexcept {return _color*_color_factor;}
			auto src_radius()const noexcept {return _src_radius;}
			auto area_of_effect()const noexcept {return _area_of_effect;}
			auto offset()const noexcept {return _offset;}
			auto brightness_factor(float f) {_color_factor = f;}
			auto shadowcaster()const noexcept {return _shadowcaster;}

		private:
			friend class Renderer_lights;

			Angle _direction;
			Angle _angle;
			Rgb _color;
			bool _shadowcaster = true;

			float _color_factor = 1.f;
			Distance _src_radius;
			Distance _area_of_effect;
			glm::vec3 _offset;

			void _update_area_of_effect();
	};

}
}
}
