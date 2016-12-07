#include "terrain_comp.hpp"

#include <core/ecs/ecs.hpp>
#include <core/ecs/serializer.hpp>
#include <core/utils/sf2_glm.hpp>
#include <core/engine.hpp>

#include <sf2/sf2.hpp>

#include <string>

namespace lux {
namespace sys {
namespace renderer {

	using namespace unit_literals;

	Terrain_comp::Terrain_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner,
	                           graphic::Material_ptr material)
	    : Component(manager,owner), _smart_texture(material, {{-1,-1}, {1,-1}, {1,1}, {-1,1}}) {
	}

	void load_component(ecs::Deserializer& state, Terrain_comp& comp) {
		std::string material = comp._smart_texture.material() ? comp._smart_texture.material().aid().str()
		                                                      : "";
		auto shadowcaster = comp._smart_texture.shadowcaster();
		auto decals_intensity = comp._smart_texture.decals_intensity();

		state.read_virtual(
			sf2::vmember("material", material),
			sf2::vmember("shadowcaster", shadowcaster),
			sf2::vmember("decals_intensity", decals_intensity)
		);

		comp._smart_texture.shadowcaster(shadowcaster);
		comp._smart_texture.decals_intensity(decals_intensity);
		comp._smart_texture.material(comp.manager().userdata().assets().load<graphic::Material>(asset::AID(material)));
		INVARIANT(comp._smart_texture.material(), "Material '"<<material<<"' not found");
	}

	void save_component(ecs::Serializer& state, const Terrain_comp& comp) {
		std::string material = comp._smart_texture.material() ? comp._smart_texture.material().aid().str()
		                                                      : "";

		state.write_virtual(
			sf2::vmember("material", material),
			sf2::vmember("shadowcaster", comp._smart_texture.shadowcaster()),
			sf2::vmember("decals_intensity", comp._smart_texture.decals_intensity())
		);
	}

	void load_component(ecs::Deserializer& state, Terrain_data_comp& comp) {
		std::vector<glm::vec2> points;
		points.reserve(16);

		state.read_virtual(
			sf2::vmember("points", points)
		);

		if(!points.empty()) {
			auto& terrain = comp.owner().get<Terrain_comp>().get_or_throw();
			terrain.smart_texture().points(std::move(points));
		}
	}

	void save_component(ecs::Serializer& state, const Terrain_data_comp& comp) {
		auto& terrain = comp.owner().get<Terrain_comp>().get_or_throw();

		state.write_virtual(
			sf2::vmember("points", terrain.smart_texture().points())
		);
	}

}
}
}
