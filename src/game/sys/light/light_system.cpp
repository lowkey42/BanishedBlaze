#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#define GLM_SWIZZLE

#include "light_system.hpp"

#include "../physics/transform_comp.hpp"

#include <core/ecs/ecs.hpp>

#include <core/renderer/command_queue.hpp>
#include <core/renderer/primitives.hpp>
#include <core/renderer/texture_batch.hpp>
#include <core/renderer/graphics_ctx.hpp>


#define NUM_LIGHTS_MACRO 4

namespace lux {
namespace sys {
namespace light {

	using namespace renderer;

	namespace {
		constexpr const auto max_lights = 4;
		static_assert(NUM_LIGHTS_MACRO==max_lights, "Update the NUM_LIGHTS_MACRO macro!");

		constexpr auto shadowed_lights = 4;
		constexpr auto shadowmap_size = 512;
		constexpr auto shadowmap_rows = shadowed_lights;

//		constexpr auto blur_min_quality = 0.25f;


		auto create_occlusion_framebuffer(Graphics_ctx& graphics_ctx) {
			auto framebuffer = Framebuffer(graphics_ctx.settings().width,
			                               graphics_ctx.settings().height);
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB);
			framebuffer.build();

			return framebuffer;
		}
		auto create_distance_framebuffer(Graphics_ctx&) {
			auto shadowmap_size_factor = 1.0f; // TODO: get from settings
			auto framebuffer = Framebuffer(int(shadowmap_size*shadowmap_size_factor), shadowmap_rows);
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGBA_16F, true);
			framebuffer.build();

			return framebuffer;
		}
		auto create_shadow_framebuffer(Graphics_ctx& graphics_ctx) {
			auto shadowmap_size_factor = 0.25f; // TODO: get from settings
			auto framebuffer = Framebuffer(int(graphics_ctx.settings().width  * shadowmap_size_factor),
			                               int(graphics_ctx.settings().height * shadowmap_size_factor));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGBA_16F);
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

	Light_system::Light_system(
	             Graphics_ctx& graphics_ctx,
	             ecs::Entity_manager& entity_manager,
	             asset::Asset_manager& asset_manager,
	             Scene_light_cfg scene_settings)
	    : _graphics_ctx(graphics_ctx),
	      _lights(entity_manager.list<Light_comp>()),
	      _scene_settings(scene_settings),
	      _light_volumn_obj(light_volumn_vertex_layout, create_buffer(create_unit_circle_mesh())),

	      _occluder_queue(1),
	      _occlusion_map(create_occlusion_framebuffer(graphics_ctx)),
	      _distance_map(create_distance_framebuffer(graphics_ctx)),
	      _shadow_map_tmp(create_shadow_framebuffer(graphics_ctx)) {

		entity_manager.register_component_type<Light_comp>();

		_shadow_maps.reserve(shadowed_lights);
		for(auto i=0; i<shadowed_lights; i++) {
			_shadow_maps.emplace_back(create_shadow_framebuffer(graphics_ctx));

			const auto unit = int(Texture_unit::shadowmap_0) + i;
			_shadow_maps.back().get_attachment("color"_strid).bind(unit);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		_relevant_lights.resize(max_lights * 4);

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
		        .bind_all_attribute_locations(renderer::simple_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "occlusions", 0,
		            "shadowmap_size", shadowmap_size
		        ));

		_shadow_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:shadowfinal"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadowfinal"_aid))
		        .bind_all_attribute_locations(renderer::simple_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "distance_map_tex", 0,
		            "occlusions", 1
		        ));

		_light_volumn_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:light_volumn"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:light_volumn"_aid))
		        .bind_all_attribute_locations(light_volumn_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "shadowmap_tex", 0,
		            "depth_tex", int(Texture_unit::height)
		        ));

		auto& shadowmap_tex = _shadow_map_tmp.get_attachment("color"_strid);
		_blur_shader.attach_shader(asset_manager.load<Shader>("vert_shader:shadow_blur"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadow_blur"_aid))
		        .bind_all_attribute_locations(renderer::simple_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "texture", 0,
		            "texture_size", glm::vec2(shadowmap_tex.width(), shadowmap_tex.height())
		        ));
	}


	void Light_system::pre_draw(std::shared_ptr<renderer::IUniform_map> uniforms_ptr,
	                            const Camera& camera, float quality) {
		_occluder_queue.shared_uniforms(uniforms_ptr);

		auto& uniforms = *uniforms_ptr;

		auto dt = Disable_depthtest{};
		auto dw = Disable_depthwrite{};


		// update camera position
		auto eye_pos = camera.eye_position();
		if(glm::length2(eye_pos-_light_cam_pos)>1.f) {
			_light_cam_pos = eye_pos;
			_light_cam_pos.z += 2.f + 5.f; // compensates for screen-space technique limitations
		}

		_collect_lights();
		_set_uniforms(uniforms, camera, quality);

		_flush_occlusion_map();
		uniforms.emplace("vp", camera.vp());
		uniforms.emplace("vp_inv", glm::inverse(camera.vp()));

		auto db = Disable_blend{};
		_render_shadow_maps(quality);
		_blur_shadows_maps(quality);

		uniforms.emplace("vp", _light_vp);

		_prev_quality = quality;

		debug_draw_framebuffer(_occlusion_map);

		for(auto& shadow_map : _shadow_maps)
			debug_draw_framebuffer(shadow_map);
	}

	void Light_system::draw_light_volumns(Command_queue& queue, const Texture& last_depth) {
		auto cmd = create_command();
		cmd.order_dependent();
		cmd.require(Gl_option::blend);
		cmd.require_not(Gl_option::depth_test);
		cmd.require_not(Gl_option::depth_write);
		cmd.shader(_light_volumn_shader);
		cmd.texture(Texture_unit::height, last_depth);
		cmd.object(_light_volumn_obj);

		for(auto i=0; i<max_lights; i++) {
			cmd.uniforms().emplace("current_light_index", i);
			if(i<int(_shadow_maps.size())) {
				cmd.texture(Texture_unit::temporary, _shadow_maps[std::size_t(i)].get_attachment("color"_strid));
			}

			queue.push_back(cmd);
		}
	}


	void Light_system::_collect_lights() {
		auto& relevant_lights = _relevant_lights;
		relevant_lights.clear();

		for(Light_comp& light : _lights) {
			auto& trans = light.owner().get<physics::Transform_comp>().get_or_throw();

			auto r = light.radius().value();
			auto dist = glm::distance2(remove_units(trans.position()).xy(), _light_cam_pos.xy());

			auto score = 1.f/dist;
			if(dist<10.f)
				score += glm::clamp(r/2.f + glm::length2(light.color())/4.f, -0.001f, 0.001f) + (light.shadowcaster() ? 0.1f : 0.f);

			relevant_lights.emplace_back(&trans, &light, score, light.shadowcaster());
		}

		std::sort(relevant_lights.begin(), relevant_lights.end());

		//< only keep the brightest/nearest N lights
		relevant_lights.resize(max_lights);

		//< shadow casting lights needs to be first
		std::stable_partition(relevant_lights.begin(), relevant_lights.end(), [](auto& l) {
			return l.shadowcaster;
		});
	}

	void Light_system::_set_uniforms(IUniform_map& uniforms, const Camera& camera, float quality) {
		auto view = camera.view();
		view[3] = glm::vec4(-_light_cam_pos, 1.f);
		_light_vp = camera.proj() * view;
		_vp = camera.vp();

		uniforms.emplace("vp", _light_vp);
		uniforms.emplace("vp_light", _light_vp);

		uniforms.emplace("light_ambient",   _scene_settings.ambient_light);
		uniforms.emplace("light_sun.color", _scene_settings.dir_light_color);
		uniforms.emplace("light_sun.dir",   _scene_settings.dir_light_direction);
		uniforms.emplace("light_quality",   quality);


		// project lights to the z=0 plane to get the position the occlusionmap needs to be sampled at
		for(auto& l : _relevant_lights) {
			if(!l.transform) {
				l.flat_pos = glm::vec2(1000,1000);
				continue;
			}

			auto pos = remove_units(l.transform->position())
			           + l.transform->resolve_relative(l.light->offset());
			pos.z = 0.f;
			auto p = _light_vp * glm::vec4(pos, 1.f);
			l.flat_pos = p.xy() / p.w;
		}

		auto process_angle = [&](auto a) {
			return (a.value() + glm::smoothstep(1.8f*glm::pi<float>(), 2.0f*PI, a.value())*1.0f) / 2.0f;
		};

		auto& lights = _relevant_lights;
		// TODO: fade out light color, when they left the screen
#define SET_LIGHT_UNIFORMS(N) \
		if(lights[N].light) {\
			uniforms.emplace("light["#N"].pos", remove_units(lights[N].transform->position())+lights[N].transform->resolve_relative(lights[N].light->offset()));\
\
			uniforms.emplace("light["#N"].dir", -lights[N].transform->rotation().value()\
			                                               + lights[N].light->_direction.value());\
\
			uniforms.emplace("light["#N"].angle", process_angle(lights[N].light->_angle));\
			uniforms.emplace("light["#N"].color", lights[N].light->color()); \
			uniforms.emplace("light["#N"].factors", lights[N].light->_factors);\
			uniforms.emplace("light["#N"].radius", lights[N].light->_radius.value());\
		} else {\
			uniforms.emplace("light["#N"].color", glm::vec3(0,0,0));\
		}

		M_REPEAT(NUM_LIGHTS_MACRO, SET_LIGHT_UNIFORMS);
#undef SET_LIGHT_UNIFORMS
	}

	namespace {
		void bind_light_positions(renderer::Shader_program& prog, gsl::span<detail::Light_info> lights) {
#define SET_LIGHT_UNIFORM(I) prog.set_uniform("light_positions["#I"]", lights[I].flat_pos);
			M_REPEAT(NUM_LIGHTS_MACRO, SET_LIGHT_UNIFORM);
#undef SET_LIGHT_UNIFORM
		}
	}

	void Light_system::_flush_occlusion_map() {
		_occlusion_map.bind_target();
		_occlusion_map.set_viewport();
		_occlusion_map.clear({0,0,0}, false);

		_occluder_queue.flush();
	}

	void Light_system::_render_shadow_maps(float quality) {
		_distance_shader.bind();
		_occlusion_map_texture->bind(1);
		_distance_shader.set_uniform("light_quality", quality);
		bind_light_positions(_distance_shader, _relevant_lights);

		_distance_map.bind_target();
		_distance_map.set_viewport();

		renderer::draw_fullscreen_quad(*_occlusion_map_texture);
	}

	void Light_system::_blur_shadows_maps(float quality) {
		if(_shadow_maps.empty())
			return;

		_shadow_shader.bind();
		_shadow_shader.set_uniform("vp", _vp);
		_shadow_shader.set_uniform("vp_inv", glm::inverse(_vp));
		_shadow_shader.set_uniform("vp_light", _light_vp);

		_shadow_maps.front().set_viewport();

		for(auto i=0; i<shadowed_lights; i++) {
			_shadow_maps.at(std::size_t(i)).bind_target();

			_shadow_shader.set_uniform("center_lightspace", _relevant_lights.at(std::size_t(i)).flat_pos);
			_shadow_shader.set_uniform("current_light_index", i);
			renderer::draw_fullscreen_quad(*_distance_map_texture);
		}

		_blur_shader.bind();
		for(auto& shadow_map : _shadow_maps) {
			_shadow_map_tmp.bind_target();
			_blur_shader.set_uniform("horizontal", false);
			renderer::draw_fullscreen_quad(shadow_map.get_attachment("color"_strid));

			shadow_map.bind_target();
			_blur_shader.set_uniform("horizontal", true);
			renderer::draw_fullscreen_quad(_shadow_map_tmp.get_attachment("color"_strid));
		}
	}

}
}
}
