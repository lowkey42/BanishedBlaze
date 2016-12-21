#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#define GLM_SWIZZLE

#include "renderer_lights.hpp"

#include "renderer_system.hpp"

#include "../physics/transform_comp.hpp"

#include <core/ecs/ecs.hpp>

#include <core/graphic/command_queue.hpp>
#include <core/graphic/primitives.hpp>
#include <core/graphic/texture_batch.hpp>
#include <core/graphic/graphics_ctx.hpp>


namespace lux {
namespace sys {
namespace renderer {

	using namespace graphic;
	using namespace unit_literals;

	namespace {
		constexpr auto shadowed_lights = 4;
		constexpr auto shadowmap_size = 512;
		constexpr auto shadowmap_rows = shadowed_lights;


		auto create_occlusion_framebuffer(Graphics_ctx& graphics_ctx) {
			auto framebuffer = Framebuffer(graphics_ctx.settings().width,
			                               graphics_ctx.settings().height);
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB_565);
			framebuffer.build();

			return framebuffer;
		}
		auto create_distance_framebuffer(Graphics_ctx&) {
			auto framebuffer = Framebuffer(int(shadowmap_size), shadowmap_rows);
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGBA, true);
			framebuffer.build();

			return framebuffer;
		}
		auto create_shadow_framebuffer(Graphics_ctx& graphics_ctx) {
			auto shadowmap_size_factor = 0.25f; // TODO: get from settings
			auto framebuffer = Framebuffer(int(graphics_ctx.settings().width  * shadowmap_size_factor),
			                               int(graphics_ctx.settings().height * shadowmap_size_factor));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGBA_16F, true);
			framebuffer.build();

			return framebuffer;
		}

		auto create_unit_circle_mesh() -> std::vector<Simple_vertex> {
			constexpr const auto steps = 12;

			std::vector<Simple_vertex> vertices;
			vertices.reserve(steps+1);

			vertices.emplace_back(glm::vec2{0,0}, glm::vec2{.5f,.5f});

			for(int i=0; i<=steps; ++i) {
				float phi = i/float(steps);

				auto x = std::cos(phi * 2*PI);
				auto y = std::sin(phi * 2*PI);
				vertices.emplace_back(glm::vec2{x,y}, (glm::vec2{x,-y}+1.f)/2.f);
			}

			return vertices;
		}

		Vertex_layout light_volumn_vertex_layout {
			Vertex_layout::Mode::triangle_fan,
			vertex("position", &Simple_vertex::xy),
			vertex("uv", &Simple_vertex::uv)
		};
	}

	Renderer_lights::Renderer_lights(
	             Graphics_ctx& graphics_ctx,
	             ecs::Entity_manager& entity_manager,
	             asset::Asset_manager& asset_manager,
	             Scene_light_cfg scene_settings)
	    : _graphics_ctx(graphics_ctx),
	      _lights(entity_manager.list<Light_comp>()),
	      _scene_settings(scene_settings),
	      _light_volumn_obj(light_volumn_vertex_layout, create_buffer(create_unit_circle_mesh())),
	      _light_uniforms(Light_uniforms{}),

	      _occluder_queue(1),
	      _occlusion_map(create_occlusion_framebuffer(graphics_ctx)),
	      _distance_map(create_distance_framebuffer(graphics_ctx)),
	      _shadow_map_tmp(create_shadow_framebuffer(graphics_ctx)) {

		entity_manager.register_component_type<Light_comp>();

		_shadow_maps.reserve(shadowed_lights);
		for(auto i=0; i<shadowed_lights; i++) {
			_shadow_maps.emplace_back(create_shadow_framebuffer(graphics_ctx));

			_shadow_maps.back().get_attachment("color"_strid).bind(0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		_relevant_lights.resize(num_lights * 4);

		_occlusion_map_texture    = &_occlusion_map.get_attachment("color"_strid);
		_distance_map_texture     = &_distance_map.get_attachment("color"_strid);

		_occlusion_map_texture->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		_distance_map_texture->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


		_distance_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:shadowmap"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadowmap"_aid))
		        .bind_all_attribute_locations(simple_vertex_layout)
		        .build()
		        .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		        .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
		        .uniforms(make_uniform_map(
		            "occlusions", int(Texture_unit::color),
		            "shadowmap_size", shadowmap_size
		        ));

		_shadow_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:shadowfinal"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadowfinal"_aid))
		        .bind_all_attribute_locations(simple_vertex_layout)
		        .build()
		        .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		        .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
		        .uniforms(make_uniform_map(
		            "distance_map_tex", int(Texture_unit::temporary),
		            "occlusions", int(Texture_unit::color)
		        ));

		_light_volumn_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:light_volumn"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:light_volumn"_aid))
		        .bind_all_attribute_locations(light_volumn_vertex_layout)
		        .build()
		        .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		        .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
		        .uniforms(make_uniform_map(
		            "shadowmap_tex", int(Texture_unit::temporary),
		            "depth_tex", int(Texture_unit::color)
		        ));

		auto& shadowmap_tex = _shadow_map_tmp.get_attachment("color"_strid);
		_blur_shader.attach_shader(asset_manager.load<Shader>("vert_shader:shadow_blur"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadow_blur"_aid))
		        .bind_all_attribute_locations(simple_vertex_layout)
		        .build()
		        .uniform_buffer("globals", int(Uniform_buffer_slot::globals))
		        .uniform_buffer("lighting", int(Uniform_buffer_slot::lighting))
		        .uniforms(make_uniform_map(
		            "tex", int(Texture_unit::temporary),
		            "texture_size", glm::vec2(shadowmap_tex.width(), shadowmap_tex.height())
		        ));
		
		
		
		int i = 0;
		for(auto& shadow_map : _shadow_maps) {
			const auto unit = int(Texture_unit::shadowmap_0) + (i++);
			shadow_map.get_attachment("color"_strid).bind(unit);
		}
		
		_light_uniforms.bind(int(Uniform_buffer_slot::lighting));
	}


	void Renderer_lights::pre_draw(const Global_uniforms& globals) {
		auto dt = Disable_depthtest{};
		auto dw = Disable_depthwrite{};


		_collect_lights(globals);

		_light_uniforms.update([&](Light_uniforms& u) {
			_set_uniforms(u, globals);
		});

		_flush_occlusion_map();

		auto db = Disable_blend{};
		_render_shadow_maps();
		_blur_shadows_maps();

		debug_draw_framebuffer(_occlusion_map);

		for(auto& shadow_map : _shadow_maps)
			debug_draw_framebuffer(shadow_map);
		
		// FIXME: bound texture is lost after each frame!?
		int i = 0;
		for(auto& shadow_map : _shadow_maps) {
			const auto unit = int(Texture_unit::shadowmap_0) + (i++);
			shadow_map.get_attachment("color"_strid).bind(unit);
		}
	}

	void Renderer_lights::draw_light_volumns(Command_queue& queue, const Texture& last_depth) {
		auto cmd = create_command();
		cmd.order_dependent();
		cmd.require(Gl_option::blend);
		cmd.require_not(Gl_option::depth_test);
		cmd.require_not(Gl_option::depth_write);
		cmd.shader(_light_volumn_shader);
		cmd.texture(Texture_unit::color, last_depth);
		cmd.object(_light_volumn_obj);

		for(auto i=0u; i<_relevant_lights.size(); i++) {
			cmd.uniforms().emplace("current_light_index", int(i));
			if(i<_shadow_maps.size()) {
				cmd.texture(Texture_unit::temporary, _shadow_maps[std::size_t(i)].get_attachment("color"_strid));
			}

			queue.push_back(cmd);
		}
	}


	void Renderer_lights::_collect_lights(const Global_uniforms& globals) {
		auto& relevant_lights = _relevant_lights;
		relevant_lights.clear();

		for(Light_comp& light : _lights) {
			auto& trans = light.owner().get<physics::Transform_comp>().get_or_throw();

			auto r = light.area_of_effect().value();
			auto dist = glm::distance2(remove_units(trans.position()).xy(), globals.eye.xy());

			auto score = 1.f/dist;
			if(dist<10.f)
				score += glm::clamp(r/2.f + glm::length2(light.color())/4.f, -0.001f, 0.001f) + (light.shadowcaster() ? 0.1f : 0.f);

			relevant_lights.emplace_back(&trans, &light, score, light.shadowcaster());
		}

		std::sort(relevant_lights.begin(), relevant_lights.end());

		//< only keep the brightest/nearest N lights
		if(relevant_lights.size()>num_lights)
			relevant_lights.resize(num_lights);

		//< shadow casting lights needs to be first
		std::stable_partition(relevant_lights.begin(), relevant_lights.end(), [](auto& l) {
			return l.shadowcaster;
		});
	}

	void Renderer_lights::_set_uniforms(Light_uniforms& uniforms, const Global_uniforms& globals) {
		uniforms.ambient_light    = glm::vec4(_scene_settings.ambient_light);
		uniforms.directional_light = glm::vec4(_scene_settings.dir_light_color, 1.f);
		uniforms.directional_dir   = glm::vec4(_scene_settings.dir_light_direction, 0.f);


		// project lights to the z=0 plane to get the position the occlusionmap needs to be sampled at
		for(auto& l : _relevant_lights) {
			if(!l.transform) {
				l.flat_pos = glm::vec2(1000,1000);
				continue;
			}

			auto pos = remove_units(l.transform->position())
			           + l.transform->resolve_relative(l.light->offset());
			pos.z = 0.f;
			auto p = globals.sse_vp * glm::vec4(pos, 1.f);
			l.flat_pos = p.xy() / p.w;
		}

		auto i=0;
		for(auto& light : _relevant_lights) {
			auto& light_uniforms = uniforms.light[i++];
			
			auto offset = light.transform->resolve_relative(light.light->offset());
			light_uniforms.position  = glm::vec4(remove_units(light.transform->position()) + offset, 1.f);
			light_uniforms.flat_position = glm::vec4(light.flat_pos, 0,0);
			light_uniforms.direction = - light.transform->rotation().value()
			                           + light.light->_direction.value();
			
			light_uniforms.color = glm::vec4(light.light->color(),1.f);
			light_uniforms.angle = light.light->_angle;
			light_uniforms.area_of_effect = light.light->area_of_effect() / 1_m * light.transform->scale();
			light_uniforms.src_radius = light.light->src_radius() / 1_m * light.transform->scale();
		}
	}

	void Renderer_lights::_flush_occlusion_map() {
		_occlusion_map.bind_target();
		_occlusion_map.set_viewport();
		_occlusion_map.clear({0,0,0}, false);

		_occluder_queue.flush();
	}

	void Renderer_lights::_render_shadow_maps() {
		_distance_shader.bind();

		_distance_map.bind_target();
		_distance_map.set_viewport();

		draw_fullscreen_quad(*_occlusion_map_texture, Texture_unit::color);
	}

	void Renderer_lights::_blur_shadows_maps() {
		if(_shadow_maps.empty())
			return;

		_shadow_shader.bind();

		_shadow_maps.front().set_viewport();

		for(auto i=0u; i<_relevant_lights.size(); i++) {
			_shadow_maps.at(i).bind_target();

			_shadow_shader.set_uniform("center_lightspace", _relevant_lights.at(i).flat_pos);
			_shadow_shader.set_uniform("current_light_index", int(i));
			draw_fullscreen_quad(*_distance_map_texture, Texture_unit::temporary);
		}

		_blur_shader.bind();
		for(auto& shadow_map : _shadow_maps) {
			for(auto i=0; i<2; i++) {
			_shadow_map_tmp.bind_target();
			_blur_shader.set_uniform("horizontal", false);
			draw_fullscreen_quad(shadow_map.get_attachment("color"_strid), Texture_unit::temporary);

			shadow_map.bind_target();
			_blur_shader.set_uniform("horizontal", true);
			draw_fullscreen_quad(_shadow_map_tmp.get_attachment("color"_strid), Texture_unit::temporary);
			}
		}
	}

}
}
}
