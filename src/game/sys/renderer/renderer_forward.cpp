#define GLM_SWIZZLE

#include "renderer_forward.hpp"

#include "../physics/transform_comp.hpp"

#include <core/units.hpp>
#include <core/graphic/command_queue.hpp>


namespace lux {
namespace sys {
namespace renderer {

	using namespace graphic;
	using namespace unit_literals;

	namespace {
		auto build_occluder_shader(asset::Asset_manager& asset_manager) -> Shader_program {
			Shader_program prog;
			prog.attach_shader(asset_manager.load<Shader>("vert_shader:sprite_shadow"_aid))
			    .attach_shader(asset_manager.load<Shader>("frag_shader:sprite_shadow"_aid))
			    .bind_all_attribute_locations(sprite_layout)
			    .build()
			    .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
			    .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
			    .uniforms(make_uniform_map(
			                  "albedo_tex", int(Texture_unit::color),
			                  "height_tex", int(Texture_unit::height)
			    ));

			return prog;
		}
		auto build_decal_shader(asset::Asset_manager& asset_manager) -> Shader_program {
			Shader_program prog;
			prog.attach_shader(asset_manager.load<Shader>("vert_shader:decal"_aid))
			    .attach_shader(asset_manager.load<Shader>("frag_shader:decal"_aid))
			    .bind_all_attribute_locations(texture_batch_layout)
			    .build()
			    .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
			    .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
			    .uniforms(make_uniform_map(
			                  "albedo_tex", int(Texture_unit::color)
			    ));

			return prog;
		}

		auto flip(glm::vec4 uv_clip, bool vert, bool horiz) {
			return glm::vec4 {
				horiz ? uv_clip.z : uv_clip.x,
				vert  ? uv_clip.w : uv_clip.y,
				horiz ? uv_clip.x : uv_clip.z,
				vert  ? uv_clip.y : uv_clip.w
			};
		}
	}

	Renderer_forward::Renderer_forward(
	        util::Message_bus& bus,
	        ecs::Entity_manager& entity_manager,
	        asset::Asset_manager& asset_manager)
	    : _occluder_shader(build_occluder_shader(asset_manager)),
	      _decal_shader(build_decal_shader(asset_manager)),
	      _mailbox(bus),
	      _sprites(entity_manager.list<Sprite_comp>()),
	      _anim_sprites(entity_manager.list<Anim_sprite_comp>()),
	      _terrains(entity_manager.list<Terrain_comp>()),
	      _particles(entity_manager.list<Particle_comp>()),
	      _decals(entity_manager.list<Decal_comp>()),
	      _particle_renderer(asset_manager),
	      _sprite_batch(512),
	      _sprite_batch_occluder(_occluder_shader, 256),
	      _decal_batch(_decal_shader, 32, false)
	{
		entity_manager.register_component_type<Sprite_comp>();
		entity_manager.register_component_type<Anim_sprite_comp>();
		entity_manager.register_component_type<Terrain_comp>();
		entity_manager.register_component_type<Terrain_data_comp>();

		_mailbox.subscribe_to<16, 128>([&](const State_change& e) {
			this->_on_state_change(e);
		});

		_sprite_batch_occluder.ignore_order(true);
	}

	void Renderer_forward::draw()const {
		for(Sprite_comp& sprite : _sprites) {
			auto& trans = sprite.owner().get<physics::Transform_comp>().get_or_throw();

			auto decal_offset = glm::vec2{};
			if(sprite._decals_sticky) {
				decal_offset.x = sprite._decals_position.x - trans.position().x.value();
				decal_offset.y = sprite._decals_position.y - trans.position().y.value();
			}

			auto position = remove_units(trans.position());
			auto sprite_data = graphic::Sprite{
			                   position, trans.rotation(),
			                   sprite._size*trans.scale(),
			                   flip(glm::vec4{0,0,1,1}, trans.flip_vertical(), trans.flip_horizontal()),
			                   sprite._shadowcaster ? 1.0f : 1.f-sprite._shadow_receiver,
			                   sprite._decals_intensity, *sprite._material, decal_offset};

			sprite_data.hue_change = {
			    sprite._hue_change_target / 360_deg,
			    sprite._hue_change_replacement / 360_deg
			};

			_sprite_batch.insert(sprite_data);

			if(sprite._shadowcaster && std::abs(position.z) < 1.0f) {
				_sprite_batch_occluder.insert(sprite_data);
			}
		}

		for(Anim_sprite_comp& sprite : _anim_sprites) {
			auto& trans = sprite.owner().get<physics::Transform_comp>().get_or_throw();

			auto decal_offset = glm::vec2{};
			if(sprite._decals_sticky) {
				decal_offset.x = sprite._decals_position.x - trans.position().x.value();
				decal_offset.y = sprite._decals_position.y - trans.position().y.value();
			}

			auto position = remove_units(trans.position());
			auto sprite_data = graphic::Sprite{
			                   position, trans.rotation(),
			                   sprite._size*trans.scale(),
			                   flip(sprite.state().uv_rect(), trans.flip_vertical(), trans.flip_horizontal()),
			                   sprite._shadowcaster ? 1.0f : 1.f-sprite._shadow_receiver,
			                   sprite._decals_intensity, sprite.state().material(), decal_offset};

			sprite_data.hue_change = {
			    sprite._hue_change_target / 360_deg,
			    sprite._hue_change_replacement / 360_deg
			};

			_sprite_batch.insert(sprite_data);

			if(sprite._shadowcaster && std::abs(position.z) < 1.0f) {
				_sprite_batch_occluder.insert(sprite_data);
			}
		}

		for(Terrain_comp& terrain : _terrains) {
			auto& trans = terrain.owner().get<physics::Transform_comp>().get_or_throw();
			auto position = remove_units(trans.position());

			terrain._smart_texture.draw(position, _sprite_batch);

			if(terrain._smart_texture.shadowcaster() && std::abs(position.z) < 1.0f) {
				terrain._smart_texture.draw(position, _sprite_batch_occluder);
			}
		}
	}

	void Renderer_forward::flush_occluders(graphic::Command_queue& queue) {
		_sprite_batch_occluder.flush(queue);
	}
	void Renderer_forward::flush_objects(graphic::Command_queue& queue) {
		_sprite_batch.flush(queue);
		_particle_renderer.draw(queue);
	}


	void Renderer_forward::draw_decals(graphic::Command_queue& queue)const {
		for(Decal_comp& d : _decals) {
			auto& trans = d.owner().get<physics::Transform_comp>().get_or_throw();
			auto pos = remove_units(trans.position()).xy();
			_decal_batch.insert(*d._texture,
			                    pos,
			                    d._size*trans.scale(),
			                    trans.rotation());
		}

		_decal_batch.flush(queue, true);
	}

	void Renderer_forward::update(Time dt) {
		auto update_decal_pos = [&](auto& sprite) {
			if(sprite._decals_sticky && !sprite._decals_position_set) {
				sprite._decals_position_set = true;
				ecs::Entity_facet o = sprite.owner();
				auto pos = remove_units(o.get<physics::Transform_comp>().get_or_throw().position());
				sprite._decals_position.x = pos.x;
				sprite._decals_position.y = pos.y;
			}
		};

		for(Anim_sprite_comp& sprite : _anim_sprites) {
			sprite.state().update(dt, _mailbox.bus());

			update_decal_pos(sprite);
		}
		for(Sprite_comp& sprite : _sprites) {
			update_decal_pos(sprite);
		}

		_update_particles(dt);
	}
	void Renderer_forward::_update_particles(Time dt) {
		for(Particle_comp& particle : _particles) {
			for(auto& id : particle._add_queue) {
				if(id!=""_strid) {
					auto existing = std::find_if(particle._emitters.begin(), particle._emitters.end(),
					                            [&](auto& e){return e && e->type()==id;});

					if(existing==particle._emitters.end()) {
						auto empty = std::find(particle._emitters.begin(), particle._emitters.end(),
						                       graphic::Particle_emitter_ptr{});
						if (empty == particle._emitters.end())
							empty = particle._emitters.begin();

						*empty = _particle_renderer.create_emiter(id);
					}
					id = ""_strid;
				}
			}

			auto& transform = particle.owner().get<physics::Transform_comp>().get_or_throw();
			auto position = remove_units(transform.position()) + particle._offset;
			for(auto& e : particle._emitters) {
				if(e) {
					e->hue_change_out(particle._hue_change);
					e->position(position);
					e->direction(glm::vec3(0,0,transform.rotation().value()));
					e->scale(transform.scale());
				}
			}
		}

		_particle_renderer.update(dt);
	}

	void Renderer_forward::post_load() {
		_particle_renderer.clear();

		// create initial emiters
		_update_particles(0_s);

		// warmup emiters
		for(auto i : util::range(4*30.f)) {
			(void)i;

			_particle_renderer.update(1_s/30.f);
		}
	}

	void Renderer_forward::_on_state_change(const State_change& s) {
	}

}
}
}
