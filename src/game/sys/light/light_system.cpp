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
		constexpr auto shadowmap_size = 1024*4;
		constexpr auto shadowmap_rows = shadowed_lights;

		constexpr auto blur_min_quality = 0.25f;


		auto create_occlusion_framebuffer(Graphics_ctx& graphics_ctx) {
			auto framebuffer = Framebuffer(graphics_ctx.settings().width,
			                               graphics_ctx.settings().height);
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB);
			framebuffer.build();

			return framebuffer;
		}
		auto create_shadowmap_framebuffer(Graphics_ctx&) {
			auto shadowmap_size_factor = 1.0f; // TODO: get from settings
			auto framebuffer = Framebuffer(int(shadowmap_size*shadowmap_size_factor), shadowmap_rows);
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
	      _shadowmaps(create_shadowmap_framebuffer(graphics_ctx)),
	      _final_shadowmaps(create_shadowmap_framebuffer(graphics_ctx)) {

		entity_manager.register_component_type<Light_comp>();

		_relevant_lights.resize(max_lights * 4);

		_occlusion_map_texture    = &_occlusion_map.get_attachment("color"_strid);
		_shadowmaps_texture       = &_shadowmaps.get_attachment("color"_strid);
		_final_shadowmaps_texture = &_final_shadowmaps.get_attachment("color"_strid);

		_shadowmaps_texture->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		_final_shadowmaps_texture->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		_raw_shadowmap_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:shadowmap"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadowmap"_aid))
		        .bind_all_attribute_locations(renderer::simple_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "occlusions", 0,
		            "shadowmap_size", shadowmap_size
		        ));

		_light_volumn_shader
		        .attach_shader(asset_manager.load<Shader>("vert_shader:light_volumn"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:light_volumn"_aid))
		        .bind_all_attribute_locations(light_volumn_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		           "shadowmaps_tex", int(Texture_unit::shadowmaps),
		           "depth_tex", int(Texture_unit::height)
		        ));

		_blur_shader.attach_shader(asset_manager.load<Shader>("vert_shader:shadow_blur"_aid))
		        .attach_shader(asset_manager.load<Shader>("frag_shader:shadow_blur"_aid))
		        .bind_all_attribute_locations(renderer::simple_vertex_layout)
		        .build()
		        .uniforms(make_uniform_map(
		            "texture", 0,
		            "texture_size", glm::vec2(shadowmap_size, shadowmap_rows*2.0)
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

		if(quality>0.f) {
			_flush_occlusion_map();
			auto db = Disable_blend{};
			_render_shadow_maps(quality);
			_blur_shadows_maps(quality);

		} else {
			FAIL("asd");
			if(glm::abs(quality-_prev_quality)>0.0001f) {
				_occluder_queue.clear();
				_final_shadowmaps.bind_target();
				_final_shadowmaps.set_viewport();
				_final_shadowmaps.clear(glm::vec3{1,1,1}, false);
			}
		}

		_final_shadowmaps_texture->bind(int(Texture_unit::shadowmaps));
		_prev_quality = quality;

		debug_draw_framebuffer(_occlusion_map);
		debug_draw_framebuffer(_shadowmaps);
		debug_draw_framebuffer(_final_shadowmaps);
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
		_raw_shadowmap_shader.bind();
		_raw_shadowmap_shader.set_uniform("light_quality", quality);
		bind_light_positions(_raw_shadowmap_shader, _relevant_lights);

		// skip blur step for low quality
		if(quality >= blur_min_quality) {
			_shadowmaps.bind_target();
			_shadowmaps.set_viewport();
		} else {
			_final_shadowmaps.bind_target();
			_final_shadowmaps.set_viewport();
		}

		renderer::draw_fullscreen_quad(*_occlusion_map_texture);
	}

	void Light_system::_blur_shadows_maps(float quality) {
		if(quality < blur_min_quality)
			return;

		_blur_shader.bind();
		_final_shadowmaps.set_viewport();
/*
		for(auto i=0; i<int(quality*2.f); i++) {
			_final_shadowmaps.bind_target();
			renderer::draw_fullscreen_quad(*_shadowmaps_texture);

			_shadowmaps.bind_target();
			renderer::draw_fullscreen_quad(*_final_shadowmaps_texture);
		}
*/
		_final_shadowmaps.bind_target();
		renderer::draw_fullscreen_quad(*_shadowmaps_texture);
	}

}
}
}
