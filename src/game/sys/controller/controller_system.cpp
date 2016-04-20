#define GLM_SWIZZLE

#include "controller_system.hpp"

#include "../physics/physics_comp.hpp"
#include "../physics/transform_comp.hpp"
#include "../physics/physics_system.hpp"

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

	bool Controller_system::_jump_allowed(Input_controller_comp& c, float ground_dist)const {
		return true;
	}

	void Controller_system::update(Time dt) {
		_mailbox.update_subscriptions();

		// TODO: AI

		auto effective_move = _move_dir;
		if(_move_left>0)
			effective_move-=1;
		if(_move_right>0)
			effective_move+=1;

		effective_move = glm::clamp(effective_move, -1.f, 1.f);

		for(Input_controller_comp& c : _input_controllers) {
			auto& body = c.owner().get<physics::Dynamic_body_comp>().get_or_throw();
			auto& ctransform = c.owner().get<physics::Transform_comp>().get_or_throw();
			auto cpos = remove_units(ctransform.position()).xy();
//			auto dir = _mouse_look ? _target_dir-cpos : _target_dir;
			// TODO: apply dir to sprite

			if(_transform_pending) { // TODO: check if on ground
				// TODO: set animation
				body.active(false); // disable player collision
				continue;
			}
			if(_transform) {
				// TODO: toggle transformation
			}
			body.active(true); // TODO: false for light form


			// TODO: cleanup this mess!!

			auto ground =_physics_world.raycast(cpos, vec2{0,-1}, 20.f, c.owner());
			auto ground_dist   = ground.process(999.f, [](auto& m) {return m.distance;});
			auto ground_normal = ground.process(vec2{0,1}, [](auto& m) {return m.normal;});
			auto move_force = vec2{0,0};
			auto grounded = true;
			auto walking = false;

			if(ground_dist>1.0f) { // we are in the air
				c._air_time += dt;
				ctransform.rotation(Angle{glm::mix(ctransform.rotation().value(), 0.f, 20*dt.value())});

				walking = glm::abs(effective_move)>0.01f;

			} else { // we are on the ground
				c._air_time = 0_s;

				auto ground_angle = Angle{glm::atan(-ground_normal.x, ground_normal.y)};

				ctransform.rotation(Angle{glm::mix(ctransform.rotation().value(), ground_angle.value()*0.75f, 10*dt.value())});

				if(abs(ground_angle)<40_deg) {
					walking = glm::abs(effective_move)>0.01f;

				} else {
					grounded = false;
				}
			}

			if(walking) {
				if(c._last_velocity!=0 && glm::sign(effective_move)!=glm::sign(c._last_velocity)) {
					c._moving_time = 0_s;
				}
				c._last_velocity = effective_move;
				c._moving_time += dt;

				auto vel_target = effective_move * (ground_dist>1.0f ? c._air_velocity : c._ground_velocity);
				vel_target *= std::min(1.f, c._moving_time.value()/c._acceleration_time);
				c._last_velocity = vel_target;

				auto vel_diff = vel_target - body.velocity().x;

				if(glm::sign(vel_diff)==glm::sign(vel_target) || glm::abs(vel_target)<=0.001f) {
					move_force.x = vel_diff * body.mass() / dt.value();
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

			body.foot_friction(glm::length2(c._last_velocity)<0.1f);

			body.apply_force(move_force);


			if(grounded && ground_dist>1.f) {
				auto width = body.size().x;
				auto ground_l =_physics_world.raycast(cpos-vec2{width/2.f,0}, vec2{0,-1}, 20.f, c.owner());
				auto ground_r =_physics_world.raycast(cpos+vec2{width/2.f,0}, vec2{0,-1}, 20.f, c.owner());
				grounded = std::min(ground_l.process(999.f, [](auto& m) {return m.distance;}),
				                    ground_r.process(999.f, [](auto& m) {return m.distance;}) ) <= 1.f;
			}


			if(_jump && c._jump_cooldown_timer<=0_s && (grounded || c._air_time<0.05_s)) {
				c._jump_cooldown_timer = c._jump_cooldown * 1_s;
				body.velocity({body.velocity().x, c._jump_velocity});
			}
			if(!_jump && grounded) {
				c._jump_cooldown_timer -= dt;
			}
		}

		_transform = false;
	}

}
}
}
