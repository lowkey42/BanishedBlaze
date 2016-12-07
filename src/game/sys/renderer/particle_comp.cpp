#define BUILD_SERIALIZER

#include "particle_comp.hpp"

#include <core/ecs/serializer.hpp>
#include <core/utils/sf2_glm.hpp>

#include <sf2/sf2.hpp>

#include <string>


namespace lux {
namespace sys {
namespace renderer {

	using namespace unit_literals;

	void load_component(ecs::Deserializer& state, Particle_comp& comp) {
		auto types = std::vector<graphic::Particle_type_id>{};
		auto hue_change = comp._hue_change.in_degrees();

		state.read_virtual(
			sf2::vmember("offset", comp._offset),
			sf2::vmember("emitters", types),
			sf2::vmember("hue_change", hue_change)
		);

		comp._hue_change = hue_change * 1_deg;

		if(!types.empty()) {
			for(auto& e : comp._emitters) {
				e.reset();
			}
			for(auto& t : types)
				comp.add(t);
		}
	}

	void save_component(ecs::Serializer& state, const Particle_comp& comp) {
		auto types = std::vector<graphic::Particle_type_id>{};
		types.reserve(comp._emitters.size());
		for(auto& e : comp._emitters) {
			if(e) {
				types.emplace_back(e->type());
			}
		}
		auto hue_change = comp._hue_change.in_degrees();

		state.write_virtual(
			sf2::vmember("offset", comp._offset),
			sf2::vmember("emitters", types),
			sf2::vmember("hue_change", hue_change)
		);
	}

	void Particle_comp::add(graphic::Particle_type_id id) {
		for(auto& q : _add_queue) {
			if(q==""_strid) {
				q = id;
				return;
			}
		}
	}
	void Particle_comp::remove(graphic::Particle_type_id id) {
		for(auto& e : _emitters) {
			if(e && id == e->type()) {
				e.reset();
				return;
			}
		}
	}

}
}
}
