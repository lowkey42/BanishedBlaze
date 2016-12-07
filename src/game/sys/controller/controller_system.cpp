#define GLM_SWIZZLE

#include "controller_system.hpp"

#include "../cam/camera_target_comp.hpp"

#include "../physics/physics_comp.hpp"
#include "../physics/transform_comp.hpp"
#include "../physics/physics_system.hpp"

#include "../gameplay/enlightened_comp.hpp"
#include "../gameplay/player_tag_comp.hpp"

#include "../renderer/sprite_comp.hpp"

#include <core/input/events.hpp>
#include <core/input/input_manager.hpp>


namespace lux {
namespace sys {
namespace controller {

	using namespace unit_literals;
	using namespace glm;

	namespace {
		constexpr auto air_dash_delay = 0.25_s;
	}

	Controller_system::Controller_system(Engine& engine, ecs::Entity_manager& ecs,
	                                     physics::Physics_system& physics_world)
	    : _mailbox(engine.bus()),
	      _input_manager(engine.input()),
	      _ecs(ecs),
	      _input_controllers(ecs.list<Input_controller_comp>()),
	      _ai_controllers(ecs.list<Ai_patrolling_comp>()),
	      _physics_world(physics_world) {

		_mailbox.subscribe_to([&](input::Once_action& e) {
			switch(e.id) {
				case "next_player"_strid:
					_switch_controller(true);
					break;
				case "prev_player"_strid:
					_switch_controller(false);
					break;
			}
		});

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
					break;

				case "transform"_strid:
					_transform_pending = e.begin;
					if(!e.begin) {
						_transform = true;
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
	                              float effective_move, bool grounded, Time dt) {
		auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();

		auto ground_normal = body.ground_normal();
		auto ground_angle = Angle{glm::atan(-ground_normal.x, ground_normal.y)};

		auto move_force = vec2{0,0};
		auto walking = glm::abs(effective_move)>0.01f;
		auto target_rotation = grounded ? ground_angle.value()*0.75f : 0.f;

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

	void Controller_system::_switch_controller(bool next) {
		if(_input_controllers.empty()) {
			_active_controlled_entity.reset();
			return;
		}

		if(!_active_controlled_entity || !_active_controlled_entity.has<Input_controller_comp>()) {
			if(_active_controlled_idx>=int(_input_controllers.size()))
				_active_controlled_idx = (_active_controlled_idx-1) % _input_controllers.size();

			auto idx=0;
			for(auto it=_input_controllers.begin(); it!=_input_controllers.end(); it++,idx++) {
				if(idx<=_active_controlled_idx) {
					_active_controlled_entity = it->owner();
					_active_controlled_idx = idx;
					return;
				}
			}
		}

		auto idx=0;
		for(auto it=_input_controllers.begin(); it!=_input_controllers.end(); it++,idx++) {
			if(!_active_controlled_entity) {
				_active_controlled_entity = it->owner();
				_active_controlled_idx = idx;
				break;
			}

			if(_active_controlled_entity.handle()==it->owner_handle()) {
				if(next) {
					it++;
					idx++;
					if(it==_input_controllers.end()) {
						it = _input_controllers.begin();
						idx = 0;
					}

				} else {
					if(it==_input_controllers.begin()) {
						it = --_input_controllers.end();
						idx = _input_controllers.size()-1;
					} else {
						it--;
						idx--;
					}
				}

				_active_controlled_entity = it->owner();
				_active_controlled_idx = idx;
				break;
			}
		}
	}

	void Controller_system::update(Time dt) {
		_mailbox.update_subscriptions();

		if(!_active_controlled_entity || !_active_controlled_entity.has<Input_controller_comp>()) {
			if(_active_controlled_entity && !_active_controlled_entity.has<Input_controller_comp>()) {
				_active_controlled_entity.reset();
			}

			_switch_controller(false);
		}

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

		if(_mouse_look) {
			_target_dir = _input_manager.last_pointer_world_position(0);
		}

		// TODO: add touch-controls here

		effective_move = glm::clamp(effective_move, -1.f, 1.f);

		for(auto& c : _input_controllers) {
			if(c.owner_handle()!=_active_controlled_entity.handle()) {
				if(!c.owner().has<physics::Dynamic_body_comp>()) {
					DEBUG("no body");
					continue;
				}
				INVARIANT(c.owner().get<physics::Transform_comp>(), "no pos");
				INVARIANT(c.owner().get<physics::Dynamic_body_comp>(), "no body");
				auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();
				_move(c, body, 0, body.grounded(), dt);
			}
		}

		if(_active_controlled_entity) {
			auto controller = _active_controlled_entity.get<Input_controller_comp>();
			if(controller.is_some()) {
				auto& c = controller.get_or_throw();

				auto sprite = c.owner().get<renderer::Anim_sprite_comp>();
				auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();
				auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();
				auto cpos = remove_units(ctransform.position()).xy();
				auto dir = _mouse_look ? _target_dir-cpos : _target_dir;
				if(!_mouse_look && glm::length2(dir)>0.1f) {
					dir.y*=-1.0f;
				}

				// normalize dir to N° steps
				if(glm::length2(dir)>0.1f) {
					const auto dir_step_size = 45_deg / (_mouse_look ? 4.f : 2.f);
					auto dir_angle = Angle{glm::atan(dir.y, dir.x)};
					dir_angle = Angle::from_degrees(std::round(dir_angle.in_degrees() / dir_step_size.in_degrees()) * dir_step_size.in_degrees());
					dir = glm::rotate(glm::vec2{1,0}, dir_angle.value());
				}


				auto ground_normal = body.ground_normal();
				auto ground_angle = Angle{glm::atan(-ground_normal.x, ground_normal.y)};
				auto grounded = body.grounded() && abs(ground_angle)<40_deg;

				auto enlightened_comp = c.owner().get<gameplay::Enlightened_comp>();
				enlightened_comp >> [&](gameplay::Enlightened_comp& light) {
					bool transformation_allowed = grounded || c._air_time<0.1_s || light.can_air_transform() || light.pending();

					if(_transform && !light.was_light()) {
						light.late_untransform();
					}

					// transforming to physical
					if(_transform_pending && light.enabled()) {
						c._air_dash_timer = transformation_allowed ? air_dash_delay : dt/2.f;
						light.start_transformation();
					}

					if(c._air_dash_timer>0_s) {
						c._air_dash_timer -= dt;
						if(c._air_dash_timer<0_s) {
							c._air_dash_timer = 0_s;
							if(!_transform) {
								light.finish_transformation();
								sprite.process([&](auto& s){s.play("untransform"_strid);});
							}
						}
					}

					// transforming to light
					if (_transform_pending && transformation_allowed && light.disabled() && c._air_dash_timer<=0_s) {
						light.start_transformation();
						sprite.process([&](auto &s) {
							s.play("transform"_strid);
						});
					}
					if (_transform_pending && transformation_allowed && !light.was_light()) {
						if (glm::length2(dir) > 0.2f) {
							light.direction(dir);
						}
					}

					// execute
					if(_transform && (transformation_allowed || light.was_light())) {
						light.finish_transformation();
						c._air_dash_timer = 0_s;
						if(light.was_light()) {
							sprite.process([&](auto &s) {s.play("untransform"_strid);});
						}
					}
				};

				bool is_light = enlightened_comp.process(false, [](auto& l){return !l.disabled();});


				if(grounded && !is_light) { // we are on the ground
					c._air_time = 0_s;
				} else {
					c._air_time += dt;
				}

				if(!is_light) {
					_move(c, body, effective_move, grounded, dt);

					if(glm::abs(c._last_velocity)>0.0001f) {
						if(std::abs(effective_move)>0.f) {
							ctransform.flip_horizontal(effective_move<0.f);
						}
						sprite.process([&](auto& s){s.play_if("idle"_strid, "walk"_strid, glm::abs(c._last_velocity)/c._ground_velocity);});
					} else {
						sprite.process([&](auto& s){s.play_if("walk"_strid, "idle"_strid);});
					}

					if(_jump && c._jump_cooldown_timer<=0_s && c._air_time<=0_s) {
						sprite.process([&](auto& s){s.play("jump"_strid);});

					} else if(!body.grounded()) {
						sprite.process([&](auto& s){s.play("fall"_strid);});
					} else {
						sprite.process([&](auto& s){
							s.play_if("fall"_strid, "land"_strid);
							s.play_if("jump"_strid, "land"_strid);
						});
					}

					if(_jump) {
						_start_jump(c, body, dt);
					} else {
						_stop_jump(c, body, dt);
					}
				}

			}
		}

		_transform = false;


		for(auto& c : _ai_controllers) {
			auto&transform_comp = c.owner().get<physics::Transform_comp>().get_or_throw();
			auto position = remove_units(transform_comp.position()).xy();
			auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();

			if(!c._start_position_set) {
				c._start_position_set = true;
				c._start_position = transform_comp.position();
			}

			auto dir = (c._moving_left ? -1.f : 1.f);

			if(c._flip_horizontal_on_return)
				transform_comp.flip_horizontal(c._moving_left);
			if(c._flip_vertical_on_return)
				transform_comp.flip_vertical(c._moving_left);


			auto next_pos = position + c._velocity * dir * dt.value()*4.f;
			auto dist2 = glm::length2(remove_units(c._start_position.xy()) - next_pos);
			auto ray = _physics_world.raycast(position, glm::vec2{dir,0.f}, 1.f);
			auto turn = dist2>c._max_distance*c._max_distance;
			if(ray.is_some() && ray.get_or_throw().entity) {
				_ecs.get(ray.get_or_throw().entity).process([&](ecs::Entity_facet& e) {
					turn |= e.has<gameplay::Player_tag_comp>();
				});
			}

			if(turn) {
				c._moving_left = !c._moving_left;
				dir*=-0.5f;
			}

			auto target_vel = c._velocity * dir;
			auto vel_diff = target_vel - body.velocity();

			if(body.kinematic()) {
				body.velocity(body.velocity() + vel_diff*glm::min(1.f, dt.value()*4.f));

			} else {
				auto move_force = vel_diff * body.mass() / dt.value();
				body.foot_friction(false);
				body.apply_force(move_force);
			}
		}
	}

}
}
}
