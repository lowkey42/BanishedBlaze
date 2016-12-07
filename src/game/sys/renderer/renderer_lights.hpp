/** manages and renders lights ***********************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "light_comp.hpp"

#include <core/graphic/camera.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/framebuffer.hpp>

#include <gsl.h>
#include <vector>


namespace lux {
namespace sys {
namespace physics {
	class Transform_comp;
}
namespace renderer {


	struct Scene_light_cfg {
		Rgb       dir_light_color     = Rgb{0.5, 0.5, 0.5};
		glm::vec3 dir_light_direction = {0.1, -0.8, 0.4};
		float     ambient_light       = 0.1f;
		float     scattering          = 0.5f;
	};
	/*
	 *
		Rgba      fog                 = Rgba{0.2,0.2,0.2};
		float     fog_near            = 10.f;
		float     fog_far             = 15.f;
	*/

	namespace detail {
		struct Light_info {
			const physics::Transform_comp* transform = nullptr;
			const Light_comp* light = nullptr;
			glm::vec2 flat_pos;
			float score = 0.f;
			bool shadowcaster = false;

			Light_info() = default;
			Light_info(const physics::Transform_comp* transform,
			           const Light_comp* light,
			           float score, bool shadowcaster)
			    : transform(transform), light(light),
			      score(score), shadowcaster(shadowcaster) {}

			bool operator<(const Light_info& rhs)const noexcept {
				return score>rhs.score;
			}
		};
	}
	
	constexpr const auto num_lights = 4;
	
	struct Point_light_uniforms {
		glm::vec4 position;
		glm::vec4 flat_position;
		glm::vec4 direction;
		glm::vec4 color;
		float angle;
		float area_of_effect;
		float src_radius;
		
		float _padding;
	};
	
	struct Light_uniforms {
		glm::vec4 ambient_light;
		glm::vec4 direcional_light;
		glm::vec4 direcional_dir;
		
		Point_light_uniforms light[num_lights];
	};

	struct Global_uniforms;

	class Renderer_lights {
		public:
			Renderer_lights(graphic::Graphics_ctx& graphics_ctx,
			             ecs::Entity_manager& entity_manager,
			             asset::Asset_manager& asset_manager,
			             Scene_light_cfg scene_settings={});

			void config(const Scene_light_cfg& cfg) {
				_scene_settings = cfg;
			}
			auto& config()      noexcept {return _scene_settings;}
			auto& config()const noexcept {return _scene_settings;}

			auto& occluder_command_queue() {
				return _occluder_queue;
			}

			//< call before any lighted object is drawn
			void pre_draw(const Global_uniforms& globals);

			//< call before other post-effects
			void draw_light_volumns(graphic::Command_queue& queue,
			                        const graphic::Texture& last_depth);

		private:
			graphic::Graphics_ctx&  _graphics_ctx;
			Light_comp::Pool&        _lights;
			Scene_light_cfg          _scene_settings;

			graphic::Shader_program _distance_shader;
			graphic::Shader_program _shadow_shader;
			graphic::Shader_program _light_volumn_shader;
			graphic::Shader_program _blur_shader;
			graphic::Object         _light_volumn_obj;
			graphic::Uniform_buffer _light_uniforms;

			graphic::Command_queue            _occluder_queue;
			graphic::Framebuffer              _occlusion_map; //< all occluders
			const graphic::Texture*           _occlusion_map_texture=nullptr;
			graphic::Framebuffer              _distance_map;    //< distance to nearest occluder per light per color
			const graphic::Texture*           _distance_map_texture=nullptr;
			std::vector<graphic::Framebuffer> _shadow_maps;
			graphic::Framebuffer              _shadow_map_tmp;

			std::vector<detail::Light_info> _relevant_lights;


			void _collect_lights(const Global_uniforms& globals);
			void _set_uniforms(Light_uniforms uniforms, const Global_uniforms&);
			void _flush_occlusion_map();
			void _render_shadow_maps();
			void _blur_shadows_maps();
	};

}
}
}
