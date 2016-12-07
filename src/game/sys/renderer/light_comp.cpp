#include "light_comp.hpp"

#include <core/ecs/serializer.hpp>
#include <core/utils/sf2_glm.hpp>

namespace lux {
namespace sys {
namespace renderer {

	using namespace unit_literals;

	namespace {
		constexpr auto light_cutoff = 0.02f;
	}

	void load_component(ecs::Deserializer& state, Light_comp& comp) {
		auto direction = comp._direction / 1_deg;
		auto angle = comp._angle / 1_deg;
		auto radius = 0.f;

		state.read_virtual(
			sf2::vmember("direction", direction),
			sf2::vmember("angle", angle),
			sf2::vmember("color", comp._color),
			sf2::vmember("radius", radius),
			sf2::vmember("factors", comp._factors),
			sf2::vmember("offset", comp._offset),
			sf2::vmember("shadowcaster", comp._shadowcaster)
		);

		if(comp._radius_based && glm::length2(comp._factors)<0.0001)
			radius = remove_unit(comp._radius);


		if(std::abs(radius)>0.001) {
			// based on radius
			comp._radius_based = true;

			float a = light_cutoff;
			float d = radius;
			float x = 0.f;
			float y = 0.f;
			float z = (1-a*(d*y + x))
			          / (a*d*d);

			comp._factors = glm::vec3{
				x,y,z
			};

		} else {
			// based on factors
			comp._radius_based = false;

			if(comp._factors.z<0.00001) {
				radius = 100.f;
			} else {
				float a = light_cutoff;
				float x = comp._factors.x;
				float y = comp._factors.y;
				float z = comp._factors.z;

				radius = (glm::sqrt(a*(a*(y*y - 4*x*z) + 4*z)) + a*y)
				         / (2*a*z);
			}
		}

		comp._direction = direction * 1_deg;
		comp._angle = angle * 1_deg;
		comp._radius = radius * 1_m;
	}

	void save_component(ecs::Serializer& state, const Light_comp& comp) {
		if(comp._radius_based) {
			state.write_virtual(
				sf2::vmember("direction", comp._direction / 1_deg),
				sf2::vmember("angle", comp._angle / 1_deg),
				sf2::vmember("color", comp._color),
				sf2::vmember("radius", remove_unit(comp._radius)),
				sf2::vmember("offset", comp._offset),
				sf2::vmember("shadowcaster", comp._shadowcaster)
			);

		} else {
			state.write_virtual(
				sf2::vmember("direction", comp._direction / 1_deg),
				sf2::vmember("angle", comp._angle / 1_deg),
				sf2::vmember("color", comp._color),
				sf2::vmember("factors", comp._factors),
				sf2::vmember("offset", comp._offset),
				sf2::vmember("shadowcaster", comp._shadowcaster)
			);
		}
	}


	Light_comp::Light_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
	    : Component(manager,owner), _direction(0_deg), _angle(360_deg), _color(1,1,1),
	      _factors(1,0,1), _radius_based(false),
	      _radius(glm::sqrt(1-light_cutoff)/glm::sqrt(light_cutoff)) {
	}

}
}
}
