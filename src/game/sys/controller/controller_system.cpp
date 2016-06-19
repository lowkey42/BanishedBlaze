#define GLM_SWIZZLE

#include "controller_system.hpp"

#include "../physics/physics_comp.hpp"
#include "../physics/transform_comp.hpp"
#include "../physics/physics_system.hpp"

#include "../gameplay/enlightened_comp.hpp"

#include "../graphic/sprite_comp.hpp"

#include <core/input/events.hpp>


namespace lux {
namespace sys {
namespace controller {

	using namespace unit_literals;
	using namespace glm;

	Controller_system::Controller_system(Engine& engine, ecs::Entity_manager& ecs,
	                                     physics::Physics_system& physics_world)
	    : _mailbox(engine.bus()),
	      _input_controllers(ecs.list<Input_controller_comp>()),
	      _ai_controllers(ecs.list<Ai_simple_comp>()),
	      _physics_world(physics_world) {

		_mailbox.subscribe_to([&](input::Continuous_action& e) {
			switch(e.id) {
				case "move_left"_strid:
					_move_left += e.begin ? 1.f : -1.f;
					break;
				case "move_right"_strid:
					_move_right += e.begin ? 1.f : -1.f;
					break;

				case "jump"_strid:
					_jump = e.begin;
					if(_jump && _transform_pending) {
						_transform_canceled = true;
						_transform_pending = false;
					}
					break;

				case "transform"_strid:
					_transform_pending = e.begin;
					if(!e.begin) {
						_transform = !_transform_canceled;
						_transform_canceled = false;
					}
					break;
			}
		});
		_mailbox.subscribe_to([&](input::Range_action& e) {
			switch(e.id) {
				case "move"_strid:
					_target_dir = e.abs;
					_move_dir = e.abs.x;
					_mouse_look = false;
					break;

				case "mouse_look"_strid:
					_mouse_look = true;
					_target_dir = e.abs;
					break;
			}
		});
	}

	bool Controller_system::input_active() {
		return _input_block_remainder<=0_s && (glm::length2(_move_dir)>0.01 || _jump || _transform_pending || _move_left || _move_right);
	}

	void Controller_system::_move(Input_controller_comp& c, physics::Dynamic_body_comp& body,
	                              float effective_move, Time dt) {
		auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();

		auto ground_normal = body.ground_normal();
		auto ground_angle = Angle{glm::atan(-ground_normal.x, ground_normal.y)};
		auto grounded = body.grounded() && abs(ground_angle)<40_deg;

		auto move_force = vec2{0,0};
		auto walking = glm::abs(effective_move)>0.01f;
		auto target_rotation = 0.f;


		if(grounded) { // we are on the ground
			c._air_time = 0_s;
			target_rotation = ground_angle.value()*0.75f;
		} else {
			c._air_time += dt;
		}
		ctransform.rotation(Angle{glm::mix(normalize(ctransform.rotation()).value(), normalize(Angle{target_rotation}).value(), 10*dt.value())});

		auto same_dir = glm::sign(effective_move)==glm::sign(c._last_velocity) || glm::abs(c._last_velocity)<1.f;

		if(walking && same_dir) {
			c._moving_time += dt;

			auto vel_target = effective_move * (grounded ? c._ground_velocity : c._air_velocity);
			vel_target *= std::min(1.f, 0.1f+c._moving_time.value()/c._acceleration_time);
			c._last_velocity = vel_target;


			if(grounded) {
				auto vel_diff = vel_target - body.velocity().x;

				if(glm::sign(vel_diff)==glm::sign(vel_target) || glm::abs(vel_target)<=0.001f) {
					move_force.x = vel_diff * body.mass() / dt.value();
				}

			} else {
				auto vel_diff = vel_target - body.velocity().x;

				if(glm::sign(vel_diff)==glm::sign(vel_target) || glm::abs(vel_target)<=0.001f) {
					move_force.x = vel_diff * body.mass() / dt.value();
				}
			}

		} else {
			if(glm::abs(c._last_velocity)>0.1f) {
				// slow down
				c._last_velocity*=0.90f / (dt.value()*60.f);
				auto vel_diff = c._last_velocity - body.velocity().x;
				move_force.x = vel_diff * body.mass() / dt.value();

			} else {
				c._last_velocity = 0.f;
			}

			c._moving_time = 0_s;
		}


		if(!grounded && body.grounded()) { // its a slope
			move_force.x = glm::clamp(move_force.x ,-10.f, 10.f);
		}

		body.foot_friction(glm::length2(c._last_velocity)<0.1f);
		body.apply_force(move_force);
	}
	void Controller_system::_start_jump(Input_controller_comp& c, physics::Dynamic_body_comp& body, Time) {
		if(c._jump_cooldown_timer<=0_s && c._air_time<0.2_s) {
			c._jump_cooldown_timer = c._jump_cooldown * 1_s;
			body.velocity({body.velocity().x, c._jump_velocity});
		}
	}
	void Controller_system::_stop_jump(Input_controller_comp& c, physics::Dynamic_body_comp&, Time dt) {
		if(c._jump_cooldown_timer>0_s && c._air_time<0.01_s) {
			c._jump_cooldown_timer -= dt;
		}
	}

	void Controller_system::update(Time dt) {
		_mailbox.update_subscriptions();

		if(_input_block_remainder>0_s) {
			_input_block_remainder -= dt;
			_transform = false;
			return;
		}

		auto effective_move = _move_dir;
		if(_move_left>0)
			effective_move-=1;
		if(_move_right>0)
			effective_move+=1;

		// TODO: add touch-controls here

		effective_move = glm::clamp(effective_move, -1.f, 1.f);

		for(Input_controller_comp& c : _input_controllers) {
			auto sprite = c.owner().get<graphic::Anim_sprite_comp>();
			auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();
			auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();
			auto cpos = remove_units(ctransform.position()).xy();
			auto dir = _mouse_look ? _target_dir-cpos : _target_dir;
			if(!_mouse_look && glm::length2(dir)>0.1f) {
				dir.y*=-1.0f;
			}

			// normalize dir to N° steps
			if(glm::length2(dir)>0.1f) {
				constexpr auto dir_step_size = 22.5_deg;
				auto dir_angle = Angle{glm::atan(dir.y, dir.x)};
				dir_angle = Angle::from_degrees(std::round(dir_angle.in_degrees() / dir_step_size.in_degrees()) * dir_step_size.in_degrees());
				dir = glm::rotate(glm::vec2{1,0}, dir_angle.value());
			}

			auto enlightened_comp = c.owner().get<gameplay::Enlightened_comp>();
			enlightened_comp >> [&](gameplay::Enlightened_comp& light) {
				bool transformation_allowed = body.grounded() || light.can_air_transform() || light.pending();

				// transforming to physical
				if(_transform_pending && light.enabled()) {
					light.start_transformation();
					light.finish_transformation();
					sprite.process([&](auto& s){s.play("untransform"_strid);});
				}

				// transforming to light
				if(_transform_pending && transformation_allowed && light.disabled()) {
					light.start_transformation();
					sprite.process([&](auto& s){s.play("transform"_strid);});
				}
				if(_transform_pending && transformation_allowed && !light.was_light()) {
					if(glm::length2(dir)>0.1f) {
						light.direction(glm::normalize(dir));
					}
				}

				// jump to cancel
				if(_transform_canceled) {
					light.cancel_transformation();
					sprite.process([&](auto& s){s.play_next("untransform"_strid);});
				}

				// execute
				if(_transform && (transformation_allowed || light.was_light())) {
					light.finish_transformation();
					sprite.process([&](auto& s){s.play_next("light"_strid);});
				}
			};

			bool is_light = enlightened_comp.process(false, [](auto& l){return !l.disabled();});

			if(is_light)
				continue;

			_move(c, body, effective_move, dt);

			if(_jump) {
				_start_jump(c, body, dt);
			} else {
				_stop_jump(c, body, dt);
			}

			if(glm::abs(c._last_velocity)>0.0001f) {
				ctransform.flip_horizontal(effective_move<0.f);
				sprite.process([&](auto& s){s.play_if("idle"_strid, "walk"_strid, glm::abs(c._last_velocity)/c._ground_velocity);});
			} else {
				sprite.process([&](auto& s){s.play_if("walk"_strid, "idle"_strid);});
			}

			if(_jump && body.velocity().y>0.1f) {
				sprite.process([&](auto& s){s.play("jump"_strid);});
			} else if(!body.grounded()) {
				sprite.process([&](auto& s){s.play("fall"_strid);});
			} else {
				sprite.process([&](auto& s){
					s.play_if("fall"_strid, "land"_strid);
					s.play_if("jump"_strid, "land"_strid);
				});
			}
		}

		_transform = false;


		for(auto& c : _ai_controllers) {
			auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();
			auto cpos = remove_units(ctransform.position()).xy();
			auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();

			auto dir = (c._moving_left ? -1.f : 1.f);

			ctransform.flip_horizontal(c._moving_left);

			auto ray = _physics_world.raycast(cpos, glm::vec2{dir,0.f}, 1.f);
			if(ray.is_some()) {
				c._moving_left = !c._moving_left;
				continue;
			}

			auto target_vel = c._velocity * dir;
			auto vel_diff = target_vel - body.velocity().x;
			auto move_force = vel_diff * body.mass() / dt.value();

			body.foot_friction(false);
			body.apply_force({move_force, 0.f});
			// TODO: flip sprite
		}
	}

}
}
}
