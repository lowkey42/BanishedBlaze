#define GLM_SWIZZLE

#include "physics_system.hpp"

#include "transform_comp.hpp"

#include <Box2D/Box2D.h>

#include <unordered_set>


namespace lux {
namespace sys {
namespace physics {

	using namespace unit_literals;

	namespace {
		constexpr auto gravity_x = 0.f;
		constexpr auto gravity_y = -50.f;

		constexpr auto time_step = 1.f / 60;
		constexpr auto velocity_iterations = 10;
		constexpr auto position_iterations = 6;

		constexpr auto max_depth_offset = 2.f;
	}

	struct Physics_system::Contact_listener : public b2ContactListener {
		util::Message_bus& bus;
		Contact_listener(Engine& e) : bus(e.bus()) {}

		void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override {
			auto a = static_cast<ecs::Entity*>(contact->GetFixtureA()->GetBody()->GetUserData());
			auto b = static_cast<ecs::Entity*>(contact->GetFixtureB()->GetBody()->GetUserData());

			if(a!=nullptr && b!=nullptr) {
				auto impact = 0.f;
				for(auto i=0; i<impulse->count; ++i) {
					impact+=impulse->normalImpulses[i];
				}

				bus.send<Collision>(a, b, impact);
			}
		}
	};

	Physics_system::Physics_system(Engine& engine, ecs::Entity_manager& ecs)
	    : _bodies_dynamic(ecs.list<Dynamic_body_comp>()),
	      _bodies_static(ecs.list<Static_body_comp>()),
	      _listener(std::make_unique<Contact_listener>(engine)),
	      _world(std::make_unique<b2World>(b2Vec2{gravity_x,gravity_y})) {

		_world->SetContactListener(_listener.get());
		_world->SetAutoClearForces(false);
	}
	Physics_system::~Physics_system() {}

	void Physics_system::update(Time dt) {
		_dt_acc += dt.value();

		auto steps = static_cast<int>(_dt_acc / time_step);
		_dt_acc -= steps*time_step;

		_get_positions();
		for(auto i=0; i<steps; ++i) {
			_world->Step(time_step, velocity_iterations, position_iterations);
		}
		_world->ClearForces();
		//_set_positions(steps/(steps+1.f) + _dt_acc / time_step);// TODO: interpolation causes inconsistent slow-downs
		_set_positions(1.f);
	}

	void Physics_system::_get_positions() {
		for(auto& comp : _bodies_dynamic) {
			if(!comp._body) {
				this->update_body_shape(comp);
			}

			auto& transform = comp.owner().get<Transform_comp>().get_or_throw();
			auto pos = remove_units(transform.position());
			auto rot = comp._def.fixed_rotation ? 0.f : transform.rotation().value();
			comp._body->SetTransform(b2Vec2{pos.x, pos.y}, rot);

			comp._body->SetActive(comp._active && std::abs(pos.z) <= max_depth_offset);
		}

		for(auto& comp : _bodies_static) {
			if(!comp._body) {
				this->update_body_shape(comp);
			}

			auto& transform = comp.owner().get<Transform_comp>().get_or_throw();
			auto pos = remove_units(transform.position());
			comp._body->SetTransform(b2Vec2{pos.x, pos.y}, transform.rotation().value());

			comp._body->SetActive(comp._active && std::abs(pos.z) <= max_depth_offset);
		}
	}
	void Physics_system::_set_positions(float alpha) {
		for(Dynamic_body_comp& comp : _bodies_dynamic) {
			auto& transform = comp.owner().get<Transform_comp>().get_or_throw();

			auto b2_pos = comp._body->GetPosition();
			auto pos = glm::vec2{b2_pos.x, b2_pos.y};
			pos = glm::mix(remove_units(transform.position()).xy(), pos, alpha);
			transform.position({pos.x*1_m, pos.y*1_m, transform.position().z});
			if(!comp._def.fixed_rotation) {
				transform.rotation(Angle{comp._body->GetAngle()});
			}

			comp._update_ground_info(*this);
		}
	}

	void Physics_system::update_body_shape(Dynamic_body_comp& comp) {
		comp._update_body(*_world);
	}

	void Physics_system::update_body_shape(Static_body_comp& comp) {
		comp._update_body(*_world);
	}

	namespace {
		struct Simple_ray_callback : public b2RayCastCallback {
			float max_dist = 1.f;
			ecs::Entity* exclude = nullptr;
			util::maybe<Raycast_result> result = util::nothing();

			Simple_ray_callback(float max_dist, ecs::Entity* exclude=nullptr)
			    : max_dist(max_dist), exclude(exclude) {
			}

			float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point,
			                      const b2Vec2& normal, float32 fraction) override {
				if(fixture->IsSensor())
					return -1.f; // ignore

				auto hit_entity = static_cast<ecs::Entity*>(fixture->GetBody()->GetUserData());
				if(exclude && hit_entity==exclude)
					return -1.f; // ignore

				result = Raycast_result{glm::vec2{normal.x, normal.y}, fraction*max_dist, hit_entity};
				return fraction;
			}

			bool ShouldQueryParticleSystem(const b2ParticleSystem*) override {
				return false;
			}
		};
	}

	auto Physics_system::raycast(glm::vec2 position, glm::vec2 dir,
	                             float max_dist) -> util::maybe<Raycast_result> {
		const auto target = position + dir*max_dist;

		Simple_ray_callback callback{max_dist};
		_world->RayCast(&callback, b2Vec2{position.x, position.y}, b2Vec2{target.x, target.y});

		return callback.result;
	}
	auto Physics_system::raycast(glm::vec2 position, glm::vec2 dir, float max_dist,
	                             ecs::Entity& exclude) -> util::maybe<Raycast_result> {
		const auto target = position + dir*max_dist;

		Simple_ray_callback callback{max_dist, &exclude};
		_world->RayCast(&callback, b2Vec2{position.x, position.y}, b2Vec2{target.x, target.y});

		return callback.result;
	}

}
}
}
