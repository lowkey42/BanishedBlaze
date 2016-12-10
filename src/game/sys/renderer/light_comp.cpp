#include "light_comp.hpp"

#include <core/ecs/serializer.hpp>
#include <core/utils/sf2_glm.hpp>

namespace lux {
namespace sys {
namespace renderer {

	using namespace unit_literals;

	namespace {
		constexpr auto light_cutoff = 0.01f;
	}

	void load_component(ecs::Deserializer& state, Light_comp& comp) {
		auto direction = comp._direction / 1_deg;
		auto angle = comp._angle / 1_deg;
		auto src_radius = comp._src_radius.value();

		state.read_virtual(
			sf2::vmember("direction", direction),
			sf2::vmember("angle", angle),
			sf2::vmember("color", comp._color),
			sf2::vmember("radius", src_radius),
			sf2::vmember("offset", comp._offset),
			sf2::vmember("shadowcaster", comp._shadowcaster)
		);


		comp._direction = direction * 1_deg;
		comp._angle = angle * 1_deg;
		comp._src_radius = src_radius * 1_m;

		comp._update_area_of_effect();
	}

	void save_component(ecs::Serializer& state, const Light_comp& comp) {
		state.write_virtual(
			sf2::vmember("direction", comp._direction / 1_deg),
			sf2::vmember("angle", comp._angle / 1_deg),
			sf2::vmember("color", comp._color),
			sf2::vmember("radius", comp._src_radius.value()),
			sf2::vmember("offset", comp._offset),
			sf2::vmember("shadowcaster", comp._shadowcaster)
		);
	}


	Light_comp::Light_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
	    : Component(manager,owner), _direction(0_deg), _angle(360_deg), _color(1,1,1) {

		_update_area_of_effect();
	}

	void Light_comp::_update_area_of_effect() {
		_area_of_effect = _src_radius * (std::sqrt(glm::length(_color)/light_cutoff) - 1.f);
	}

}
}
}
