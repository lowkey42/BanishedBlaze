#define BUILD_SERIALIZER
#define GLM_SWIZZLE

#include "particles.hpp"

#include "texture.hpp"
#include "vertex_object.hpp"

#include "../utils/random.hpp"
#include "../utils/sf2_glm.hpp"

#include <vector>


namespace lux {
namespace graphic {

	using namespace unit_literals;

	sf2_structDef(Float_range,
		min, max
	)

	sf2_structDef(Attractor_plane, normal, force)
	sf2_structDef(Attractor_point, point, force)

	sf2_structDef(Particle_type,
		id,
		physics_simulation,
		mass,
		texture,

		animation_frames,
		fps,
		hue_change_in,

		initial_alpha,
		final_alpha,
		initial_opacity,
		final_opacity,

		initial_size,
		final_size,
		rotation,
		lifetime,

		emision_rate,
		max_particle_count,

		spawn_x, spawn_y, spawn_z,

		initial_pitch,
		initial_yaw,
		initial_roll,

		speed_pitch,
		speed_yaw,
		speed_roll,

		speed_pitch_global,
		speed_yaw_global,
		speed_roll_global,

		initial_speed,
		final_speed,

		attractor_plane,
		attractor_point,
		source_velocity_conservation
	)

	namespace {
		struct Particle_draw {
			glm::vec3 position;
			glm::vec3 direction;
			float rotation;
			float frames;
			float current_frame;
			float size;

			float alpha;
			float opacity;
			float hue_change_out;
		};
		Vertex_layout simple_particle_vertex_layout {
			Vertex_layout::Mode::points,
			vertex("position",      &Particle_draw::position),
			vertex("direction",     &Particle_draw::direction),
			vertex("rotation",      &Particle_draw::rotation),
			vertex("frames",        &Particle_draw::frames),
			vertex("current_frame", &Particle_draw::current_frame),
			vertex("size",          &Particle_draw::size),
			vertex("alpha",         &Particle_draw::alpha),
			vertex("opacity",       &Particle_draw::opacity),
			vertex("hue_change_out",&Particle_draw::hue_change_out)
		};

		struct Particle_sim {
			Time ttl;
			Time lifetime;

			float initial_alpha;
			float final_alpha;
			float initial_opacity;
			float final_opacity;

			float initial_size;
			float final_size;

			glm::quat source_direction;
			glm::quat direction;
			float initial_speed;
			float final_speed;

			glm::quat angular_speed_local;
			glm::quat angular_speed_global;

			glm::vec3 linear_speed;
		};

		glm::quat calculate_w(float pitch, float yaw, float roll) {
			return glm::quat(0, roll, pitch, yaw);
		}

		auto rng = util::create_random_generator();

		auto rand_val(Float_range r) {
			return util::random_real(rng, r.min, r.max);
		}
	}


	class Simple_particle_emitter : public Particle_emitter {
		public:
			Simple_particle_emitter(asset::Asset_manager& assets, const Particle_type& type)
			    : Particle_emitter(type),
			      _obj(simple_particle_vertex_layout,
			           create_dynamic_buffer<Particle_draw>(type.max_particle_count)) {

				_texture = assets.load<Texture>(asset::AID{type.texture});

				_particles_draw.reserve(type.max_particle_count * _scale);
				_particles_sim.reserve(type.max_particle_count * _scale);
			}

			auto texture()const noexcept -> const Texture* override {return &*_texture;}

			void update(Time dt) override {
				INVARIANT(_particles_draw.size()==_particles_sim.size(), "Size mismatch in particle sim/draw buffer.");

				if(!_last_position_set) {
					_last_position_set = true;
					_last_position = _position;
				}

				_dt_acc += dt;
				auto count_scale = std::max(_scale, 0.01f);
				auto spawn_now = util::random_int(rng,_type.emision_rate.min, _type.emision_rate.max) * count_scale;
				_to_spawn = static_cast<decltype(_to_spawn)>(std::round(spawn_now * _dt_acc.value()));
				_dt_acc-=Time(_to_spawn/spawn_now);
				if(!_active) {
					_to_spawn = 0;
				}

				_reap(dt);
				_spawn();

				_simulation(dt);

				if(!_particles_draw.empty())
					_obj.buffer().set(_particles_draw);

				_last_position = _position;
			}

			bool draw(Command& cmd)const override {
				if(!_particles_draw.empty()) {
					cmd.object(_obj).texture(Texture_unit::color, *_texture);
					return true;
				}
				return false;
			}

			bool dead()const noexcept override {return !_active && _particles_draw.empty();}

		private:
			Texture_ptr _texture;
			Object _obj;

			std::vector<Particle_draw> _particles_draw;
			std::vector<Particle_sim> _particles_sim;

			int_fast32_t _to_spawn;
			Time _dt_acc{0};

			glm::vec3 _last_position;
			bool _last_position_set = false;


			void _reap(Time dt) {
				auto to_spawn = _to_spawn;
				auto new_end = int_fast32_t(_particles_sim.size());

				for(auto i=0; i<new_end; i++) {
					auto& curr = _particles_sim[i];
					curr.ttl-=dt;
					auto dead = curr.ttl<=0_s;

					if(dead) {
						if(to_spawn>0) {
							_spawn_particle_at(i);
							to_spawn--;
						} else {
							std::swap(_particles_sim[i], _particles_sim[new_end-1]);
							std::swap(_particles_draw[i], _particles_draw[new_end-1]);
							i--;
							new_end--;
						}
					}
				}

				if(static_cast<std::size_t>(new_end) < _particles_sim.size()) {
					_particles_sim.erase(_particles_sim.begin()+new_end,
					                     _particles_sim.end());
					_particles_draw.erase(_particles_draw.begin()+new_end,
					                     _particles_draw.end());
				}

				_to_spawn = to_spawn;
			}

			void _spawn() {
				using IT = decltype(_to_spawn);

				auto count_scale = std::max(_scale, 0.01f);
				auto max_spawn = static_cast<IT>(_type.max_particle_count * count_scale - _particles_sim.size());
				_to_spawn = std::max(static_cast<IT>(0), std::min(_to_spawn, max_spawn));

				for(auto i : util::range(_to_spawn)) {
					(void)i;

					_particles_sim.emplace_back();
					_particles_draw.emplace_back();
					_spawn_particle_at(_particles_sim.size()-1);
				}
			}
			void _spawn_particle_at(int_fast32_t idx) {
				Particle_sim& sim = _particles_sim[idx];
				sim.ttl = sim.lifetime = rand_val(_type.lifetime) * second;
				sim.initial_alpha = rand_val(_type.initial_alpha);
				sim.final_alpha = rand_val(_type.final_alpha);
				sim.initial_opacity = rand_val(_type.initial_opacity);
				sim.final_opacity = rand_val(_type.final_opacity);
				sim.initial_size = rand_val(_type.initial_size);
				sim.final_size = rand_val(_type.final_size);
				sim.initial_speed = rand_val(_type.initial_speed);
				sim.final_speed = rand_val(_type.final_speed) ;

				sim.angular_speed_local = calculate_w(
					rand_val(_type.speed_pitch),
					rand_val(_type.speed_yaw),
					rand_val(_type.speed_roll)
				);
				sim.angular_speed_global = calculate_w(
						rand_val(_type.speed_pitch_global),
						rand_val(_type.speed_yaw_global),
						rand_val(_type.speed_roll_global)
				);
				sim.source_direction = _direction;
				sim.direction = glm::normalize(glm::quat(glm::vec3(
					rand_val(_type.initial_pitch),
					rand_val(_type.initial_yaw),
					rand_val(_type.initial_roll)
				)));
				sim.linear_speed = (_position - _last_position) * _type.source_velocity_conservation;


				Particle_draw& draw = _particles_draw[idx];
				draw.position = glm::mix(_last_position, _position, util::random_real(rng, 0.f, 1.f))
								+ glm::rotate(_direction, glm::vec3(
					rand_val(_type.spawn_x) * _scale,
					rand_val(_type.spawn_y) * _scale,
					rand_val(_type.spawn_z)
				));
				draw.direction = glm::rotate(sim.direction, glm::vec3(1,0,0));
				draw.rotation = rand_val(_type.rotation);
				draw.frames = _type.animation_frames;
				draw.current_frame = 0;
				draw.size = sim.initial_size;
				draw.alpha = sim.initial_alpha;
				draw.opacity = sim.initial_opacity;
				draw.hue_change_out = _hue_out / (360_deg).value();
			}

			void _simulation(Time dt) {
				for(std::size_t i=0; i<_particles_sim.size(); i++) {
					Particle_sim& curr_sim = _particles_sim[i];
					Particle_draw& curr_draw = _particles_draw[i];

					auto a = 1.f - curr_sim.ttl/curr_sim.lifetime;

					curr_draw.alpha = glm::mix(curr_sim.initial_alpha, curr_sim.final_alpha, a);
					curr_draw.opacity = glm::mix(curr_sim.initial_opacity, curr_sim.final_opacity, a);
					curr_draw.size = glm::mix(curr_sim.initial_size, curr_sim.final_size, a);

					if(_type.fps<0) {
						curr_draw.current_frame = (curr_draw.frames-1.0)*a;
					} else {
						auto next_frame = curr_draw.current_frame + _type.fps*dt.value();
						curr_draw.current_frame = std::fmod(next_frame, curr_draw.frames);
					}

					auto speed = glm::mix(curr_sim.initial_speed, curr_sim.final_speed, a);

					auto& q = curr_sim.direction;
					q = glm::normalize(q + dt.value()*0.5f * q * curr_sim.angular_speed_local);
					q = glm::normalize(q + dt.value()*0.5f * curr_sim.angular_speed_global * q);

					// TODO: attractors

					auto final_direction = glm::normalize(curr_sim.source_direction * q);
					auto dir = glm::rotate(final_direction, glm::vec3(1,0,0));
					curr_draw.position += dir * speed;
					curr_draw.position += curr_sim.linear_speed;

					curr_draw.direction = dir;
				}
			}
	};

	auto get_type(const Particle_emitter& e) -> Particle_type_id {
		return e.type();
	}
	void set_position(Particle_emitter& e, glm::vec3 position) {
		e.position(position);
	}

	void set_direction(Particle_emitter& e, glm::vec3 euler_angles) {
		e.direction(euler_angles);
	}

	Particle_renderer::Particle_renderer(asset::Asset_manager& assets) {
		_simple_shader.attach_shader(assets.load<Shader>("vert_shader:particles"_aid))
		              .attach_shader(assets.load<Shader>("frag_shader:particles"_aid))
		              .bind_all_attribute_locations(simple_particle_vertex_layout)
		              .build()
		              .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		              .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
		              .uniforms(make_uniform_map(
		                  "tex", int(Texture_unit::color),
		                  "shadowmap_0_tex", int(Texture_unit::shadowmap_0),
		                  "shadowmap_1_tex", int(Texture_unit::shadowmap_1),
		                  "shadowmap_2_tex", int(Texture_unit::shadowmap_2),
		                  "shadowmap_3_tex", int(Texture_unit::shadowmap_3),
		                  "environment_tex", int(Texture_unit::environment),
		                  "last_frame_tex", int(Texture_unit::last_frame)
		              ));

		auto type_aids = assets.list("particle"_strid);
		for(auto& aid : type_aids) {
			auto type = assets.load<Particle_type>(aid);
			auto id = type->id;
			_types.emplace(id, std::move(type));
		}
	}

	auto Particle_renderer::create_emiter(Particle_type_id id) -> Particle_emitter_ptr {
		auto iter = _types.find(id);
		if(iter==_types.end()) {
			WARN("Created emiter of unknown type '"<<id.str()<<"'");
			iter = _types.begin();
		}

		auto emiter = Particle_emitter_ptr{};

		if(iter->second->physics_simulation) {
			FAIL("NOT IMPLEMENTED, YET!");
		} else {
			emiter = std::make_shared<Simple_particle_emitter>(iter->second.mgr(), *iter->second);
		}

		_emitters.emplace_back(emiter);

		return emiter;
	}

	void Particle_renderer::update(Time dt) {
		for(auto& e : _emitters) {
			if(e.use_count()<=1) {
				e->disable();
			}
			e->update(dt);
		}

		_emitters.erase(std::remove_if(_emitters.begin(),_emitters.end(), [](auto& e){return e->dead();}), _emitters.end());
	}
	void Particle_renderer::draw(Command_queue& queue)const {
		for(auto& e : _emitters) {
			auto cmd = create_command().shader(_simple_shader)
					.require_not(Gl_option::depth_write)
					.require(Gl_option::depth_test)
					.require(Gl_option::blend)
					.order_dependent();

			cmd.uniforms().emplace("hue_change_in", e->hue_change_in() / (360_deg).value());

			if(e->draw(cmd))
				queue.push_back(cmd);
		}
	}

	void Particle_renderer::clear() {
		_emitters.clear();
	}

}
namespace asset {
	auto Loader<graphic::Particle_type>::load(istream in) -> std::shared_ptr<graphic::Particle_type> {
		auto r = std::make_shared<graphic::Particle_type>();

		sf2::deserialize_json(in, [&](auto &msg, uint32_t row, uint32_t column) {
			ERROR("Error parsing JSON from " << in.aid().str() << " at " << row << ":" << column << ": " <<
				  msg);
		}, *r);

		auto from_deg = [](float& v) {
			v = Angle::from_degrees(v).value();
		};

		from_deg(r->initial_pitch.min);
		from_deg(r->initial_pitch.max);
		from_deg(r->initial_yaw.min);
		from_deg(r->initial_yaw.max);
		from_deg(r->initial_roll.min);
		from_deg(r->initial_roll.max);

		from_deg(r->speed_pitch.min);
		from_deg(r->speed_pitch.max);
		from_deg(r->speed_yaw.min);
		from_deg(r->speed_yaw.max);
		from_deg(r->speed_roll.min);
		from_deg(r->speed_roll.max);

		from_deg(r->speed_pitch_global.min);
		from_deg(r->speed_pitch_global.max);
		from_deg(r->speed_yaw_global.min);
		from_deg(r->speed_yaw_global.max);
		from_deg(r->speed_roll_global.min);
		from_deg(r->speed_roll_global.max);

		from_deg(r->hue_change_in);

		return r;
	}

	void Loader<graphic::Particle_type>::store(ostream out, const graphic::Particle_type &asset) {
		auto to_deg = [](float& v) {
			v = Angle{v}.in_degrees();
		};

		auto r = asset;

		to_deg(r.initial_pitch.min);
		to_deg(r.initial_pitch.max);
		to_deg(r.initial_yaw.min);
		to_deg(r.initial_yaw.max);
		to_deg(r.initial_roll.min);
		to_deg(r.initial_roll.max);

		to_deg(r.speed_pitch.min);
		to_deg(r.speed_pitch.max);
		to_deg(r.speed_yaw.min);
		to_deg(r.speed_yaw.max);
		to_deg(r.speed_roll.min);
		to_deg(r.speed_roll.max);

		to_deg(r.speed_pitch_global.min);
		to_deg(r.speed_pitch_global.max);
		to_deg(r.speed_yaw_global.min);
		to_deg(r.speed_yaw_global.max);
		to_deg(r.speed_roll_global.min);
		to_deg(r.speed_roll_global.max);

		to_deg(r.hue_change_in);

		sf2::serialize_json(out, r);
	}
}
}
