#include "camera_target_comp.hpp"



namespace lux {
namespace sys {
namespace cam {

	void Camera_target_comp::load(sf2::JsonDeserializer& state,
	                              asset::Asset_manager& asset_mgr) {
		state.read_lambda([](auto&){return false;});
		// TODO
	}

	void Camera_target_comp::save(sf2::JsonSerializer& state)const {
		state.write_virtual();
		// TODO
	}

	Camera_target_comp::Camera_target_comp(ecs::Entity& owner) : Component(owner) {
		// TODO
	}

}
}
}
