#define GLM_SWIZZLE

#include "camera_system.hpp"

#include "../physics/transform_comp.hpp"
#include "../physics/physics_comp.hpp"

#include <core/graphic/graphics_ctx.hpp>

#include <glm/gtx/random.hpp>


namespace lux {
namespace sys {
namespace cam {

	using namespace unit_literals;

	namespace {
		constexpr auto cam_distance = 2_m;

		template<class T>
		T save_mix(const T& x, const T& y, float a) {
			return glm::mix(x,y, glm::clamp(a, 0.f, 1.f));
		}
	}

	Camera_system::Camera_system(Engine& engine, ecs::Entity_manager& ecs)
	    : _targets(ecs.list<Camera_target_comp>()),
	      _camera(engine.graphics_ctx().viewport(), 80_deg, cam_distance, 100_m) {
	}
	void Camera_system::reset_position(Position p) {
		std::fill(std::begin(_target_history), std::end(_target_history), p);
		_last_target = p;
	}

	void Camera_system::active_only(ecs::Entity_handle e) {
		for(auto& target : _targets) {
			target.active(target.owner_handle()==e);
		}
	}

	auto Camera_system::_calc_target() -> Position {
		if(_targets.size()==0) {
			return _last_target;

		} else {
			auto target = std::find_if(_targets.begin(), _targets.end(), [](auto& t) {return t.active();});
			if(target==_targets.end())
				return _last_target;

			auto& cam = *target;
			auto& transform = cam.owner().get<physics::Transform_comp>().get_or_throw();
			auto grounded = cam.owner().get<physics::Dynamic_body_comp>().process(true, [](auto& b){return b.grounded();});

			auto x = transform.position().x;
			auto y = transform.position().y;

			if(_type == Camera_move_type::lazy) {
				auto dist_y = glm::abs(remove_unit(_last_target.y-y));
				auto move_y = (grounded && dist_y>0.1f) || dist_y>6.f;
				if(!move_y) {
					y = _last_target.y;
				}

				if(glm::abs(remove_unit(_last_target.x-x))<4.f && !_moving)
					x = _last_target.x;

				_moving = glm::abs(remove_unit(_last_target.x-x))>0.01f;
			}

			return Position{x, y, cam_distance};
		}
	}

	auto Camera_system::_smooth_target(Position curr, Time dt) -> Position {
		auto sum = Position{0_m,0_m,0_m};
		for(auto v : _target_history) {
			sum += v;
		}

		_target_history_curr = (_target_history_curr+1) % _target_history.size();
		_target_history[_target_history_curr] = curr;

		curr = save_mix(remove_units(sum)/float(_target_history.size()), remove_units(curr), 0.5f)*1_m;
		return curr;
	}

	void Camera_system::start_slow_lerp(Time t) {
		_slow_lerp_time = t;
		_slow_lerp_remainder = t;
		_slow_lerp_start = _last_target;
		_slow_lerp_started = false;
	}

	void Camera_system::update(Time dt) {
		auto target = _calc_target();

		if(_first_target || _type_changed) {
			reset_position(target);
			_first_target = false;
			_type_changed = false;
		}



		if(_slow_lerp_remainder>0_s) {
			if(_targets.size()>0) {
				_slow_lerp_remainder-=dt;

				_slow_lerp_started |= _slow_lerp_remainder<_slow_lerp_time*0.9f && glm::distance2(remove_units(_slow_lerp_target), remove_units(target))<0.001f;

				if(_slow_lerp_started) {
					auto alpha = 1.f - glm::clamp(_slow_lerp_remainder/_slow_lerp_time, 0.f, 1.f);
					alpha = glm::smoothstep(0.f, 1.f, alpha);
					target = save_mix(remove_units(_slow_lerp_start), remove_units(_slow_lerp_target), alpha) * 1_m;
				} else {
					_slow_lerp_target = target;
					target = _slow_lerp_start;
				}
			}

		} else if(_type == Camera_move_type::lazy) {
			auto org_target = target;
			target.x = save_mix(_last_target.x.value(), target.x.value(), std::min(dt.value()*5.f,1.f))*1_m;
			target.y = save_mix(_last_target.y.value(), target.y.value(), std::min(dt.value()*10.f,1.f))*1_m;
			if(glm::length2(remove_units(target-org_target))<0.005f)
				target = org_target;

		} else {
			target = save_mix(remove_units(_last_target), remove_units(target), std::min(dt.value()*30.f,1.f)) * 1_m;
		}

		auto diff = remove_units(target - _last_target).xy();
		if(glm::length2(diff)>0.01f) {
			_motion_blur_dir = glm::normalize(diff);
		}

		_last_target = target;
		if(_type == Camera_move_type::lazy) {
			target = _smooth_target(target, dt);
		}

		if(_screen_shake_time_left>0_s) {
			_screen_shake_time_left -= dt;
			target += glm::sphericalRand(0.1f*_screen_shake_force) * 1_m;
		}

		_camera.position(target);
	}
	auto Camera_system::screen_to_world(glm::vec2 screen_pos) const noexcept -> glm::vec3 {
		auto exp_p = remove_units(_last_target);
		exp_p.z = 0.0;
		return _camera.screen_to_world(screen_pos, exp_p);
	}

}
}
}
