#define BUILD_SERIALIZER

#include "sprite_comp.hpp"

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

	void load_component(ecs::Deserializer& state, Sprite_comp& comp) {
		std::string aid = comp._material ? comp._material.aid().str() : "";
		auto hc_target_deg = comp._hue_change_target.in_degrees();
		auto hc_replacement_deg = comp._hue_change_replacement.in_degrees();

		state.read_virtual(
			sf2::vmember("material", aid),
			sf2::vmember("size", comp._size),
			sf2::vmember("shadowcaster", comp._shadowcaster),
			sf2::vmember("shadow_receiver", comp._shadow_receiver),
			sf2::vmember("hue_change_target", hc_target_deg),
			sf2::vmember("hue_change_replacement", hc_replacement_deg),
			sf2::vmember("decals_intensity", comp._decals_intensity),
			sf2::vmember("decals_sticky", comp._decals_sticky)
		);

		if(!aid.empty())
			comp._material = comp.manager().userdata().assets().load<graphic::Material>(asset::AID(aid));

		comp._hue_change_target = hc_target_deg * 1_deg;
		comp._hue_change_replacement = hc_replacement_deg * 1_deg;
	}

	void save_component(ecs::Serializer& state, const Sprite_comp& comp) {
		std::string aid = comp._material ? comp._material.aid().str() : "";

		state.write_virtual(
			sf2::vmember("material", aid),
			sf2::vmember("size", comp._size),
			sf2::vmember("shadowcaster", comp._shadowcaster),
			sf2::vmember("shadow_receiver", comp._shadow_receiver),
			sf2::vmember("hue_change_target", comp._hue_change_target.in_degrees()),
			sf2::vmember("hue_change_replacement", comp._hue_change_replacement.in_degrees()),
			sf2::vmember("decals_intensity", comp._decals_intensity),
			sf2::vmember("decals_sticky", comp._decals_sticky)
		);
	}



	Anim_sprite_comp::Anim_sprite_comp(ecs::Entity_manager& manager, ecs::Entity_handle owner)
	    : Component(manager, owner), _anim_state(to_void_ptr(owner)) {
	}

	void load_component(ecs::Deserializer& state, Anim_sprite_comp& comp) {
		std::string aid = comp._anim_state.animation_set() ? comp._anim_state.animation_set().aid().str() : "";
		auto anim_clip = comp._anim_state.get_clip();
		auto hc_target_deg = comp._hue_change_target.in_degrees();
		auto hc_replacement_deg = comp._hue_change_replacement.in_degrees();

		state.read_virtual(
			sf2::vmember("animation_set", aid),
			sf2::vmember("animation_clip", anim_clip),
			sf2::vmember("size", comp._size),
			sf2::vmember("shadowcaster", comp._shadowcaster),
			sf2::vmember("shadow_receiver", comp._shadow_receiver),
			sf2::vmember("hue_change_target", hc_target_deg),
			sf2::vmember("hue_change_replacement", hc_replacement_deg),
			sf2::vmember("decals_intensity", comp._decals_intensity),
			sf2::vmember("decals_sticky", comp._decals_sticky)
		);

		if(!aid.empty()) {
			auto anim_set = comp.manager().userdata().assets().load<graphic::Sprite_animation_set>(asset::AID(aid));
			INVARIANT(anim_set, "Animation_set '"<<aid<<"' not found");
			comp._anim_state.animation_set(anim_set, anim_clip);
		}

		comp._hue_change_target = hc_target_deg * 1_deg;
		comp._hue_change_replacement = hc_replacement_deg * 1_deg;
	}

	void save_component(ecs::Serializer& state, const Anim_sprite_comp& comp) {
		std::string aid = comp._anim_state.animation_set() ? comp._anim_state.animation_set().aid().str() : "";

		state.write_virtual(
			sf2::vmember("animation_set", aid),
			sf2::vmember("animation_clip", comp._anim_state.get_clip()),
			sf2::vmember("size", comp._size),
			sf2::vmember("shadowcaster", comp._shadowcaster),
			sf2::vmember("shadow_receiver", comp._shadow_receiver),
			sf2::vmember("hue_change_target", comp._hue_change_target.in_degrees()),
			sf2::vmember("hue_change_replacement", comp._hue_change_replacement.in_degrees()),
			sf2::vmember("decals_intensity", comp._decals_intensity),
			sf2::vmember("decals_sticky", comp._decals_sticky)
		);
	}

	void Anim_sprite_comp::play(graphic::Animation_clip_id id, float speed) {
		_anim_state.speed(speed);
		_anim_state.set_clip(id);
	}

	void Anim_sprite_comp::play_if(graphic::Animation_clip_id expected,
	             graphic::Animation_clip_id id, float speed) {
		if(!_anim_state.playing() || _anim_state.get_clip()==expected) {
			play(id, speed);
		}
	}

	void Anim_sprite_comp::play_next(graphic::Animation_clip_id id) {
		_anim_state.set_next_clip(id);
	}

	auto Anim_sprite_comp::playing()const noexcept -> util::maybe<graphic::Animation_clip_id> {
		return _anim_state.playing() ? util::just(_anim_state.get_clip())
		                             : util::nothing();
	}

}
}
}
