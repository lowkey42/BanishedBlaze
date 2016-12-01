/** manages and renders lights ***********************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "light_comp.hpp"

#include <core/renderer/camera.hpp>
#include <core/renderer/command_queue.hpp>
#include <core/renderer/shader.hpp>
#include <core/renderer/texture.hpp>
#include <core/renderer/framebuffer.hpp>

#include <gsl.h>
#include <vector>


namespace lux {
namespace sys {
namespace physics {
	class Transform_comp;
}
namespace light {

	/*
	 * TODO
	 *
Occlusion map: 2D RGB Texture - Fullscreen+
	Schwarz:	filtert nichts
	Rot:		filtert rot
	Weiß:		filtert alles

Raw Shadowmap pro Licht: 1D RGBA Texture - fixed width
	RGB: Abständ zum nächsten Occluder (normalisiert 0-1)
	A: Y-Moment for Variance shadowmapping
		float depth = MIN(R,G,B);
		float dx = dFdx(depth);
		A = depth*depth + 0.25*(dx*dx);

Fall Variance shadow mapping nicht wie geplannt funktioniert, müssen die Shadowmapps pro Lichtquelle getrennt gespeichert und manuell geblured werden:
			std::vector<renderer::Framebuffer> _per_light_shadowmaps; //< fullscreen shadowmap per light


		auto create_shadowmap_framebuffer(Graphics_ctx& graphics_ctx) {
			auto res = shadowmap_resolution(graphics_ctx);
			auto framebuffer = Framebuffer(int(res.x), int(res.y));
			framebuffer.add_color_attachment("color"_strid, 0, Texture_format::RGB);
			framebuffer.build();

			return framebuffer;
		}

		_per_light_shadowmaps.reserve(shadowed_lights);
		for(int i=0; i<shadowed_lights; i++) {
			_per_light_shadowmaps.emplace_back(create_shadowmap_framebuffer(graphics_ctx));
		}
	 */


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


	class Light_system {
		public:
			Light_system(renderer::Graphics_ctx& graphics_ctx,
			             ecs::Entity_manager& entity_manager,
			             asset::Asset_manager& asset_manager,
			             Scene_light_cfg scene_settings={});

			void config(const Scene_light_cfg& cfg) {
				_scene_settings = cfg;
			}
			auto& config() noexcept {return _scene_settings;}
			auto& config()const noexcept {return _scene_settings;}

			auto& occluder_command_queue() {
				return _occluder_queue;
			}

			//< call before any lighted object is drawn
			void pre_draw(std::shared_ptr<renderer::IUniform_map> uniforms,
			              const renderer::Camera& camera, float quality=1.f);

			//< call before other post-effects
			void draw_light_volumns(renderer::Command_queue& queue, const renderer::Texture& last_depth);

		private:
			renderer::Graphics_ctx&  _graphics_ctx;
			Light_comp::Pool&        _lights;
			Scene_light_cfg          _scene_settings;

			renderer::Shader_program _distance_shader;
			renderer::Shader_program _shadow_shader;
			renderer::Shader_program _light_volumn_shader;
			renderer::Shader_program _blur_shader;
			renderer::Object         _light_volumn_obj;

			renderer::Command_queue            _occluder_queue;
			renderer::Framebuffer              _occlusion_map; //< all occluders
			const renderer::Texture*           _occlusion_map_texture=nullptr;
			renderer::Framebuffer              _distance_map;    //< distance to nearest occluder per light per color
			const renderer::Texture*           _distance_map_texture=nullptr;
			std::vector<renderer::Framebuffer> _shadow_maps;
			renderer::Framebuffer              _shadow_map_tmp;

			float     _prev_quality = -1;
			glm::vec3 _light_cam_pos;
			glm::mat4 _light_vp;
			glm::mat4 _vp;

			std::vector<detail::Light_info> _relevant_lights;


			void _collect_lights();
			void _set_uniforms(renderer::IUniform_map& uniforms,
			                   const renderer::Camera& camera,
			                   float quality);
			void _flush_occlusion_map();
			void _render_shadow_maps(float quality);
			void _blur_shadows_maps(float quality);
	};

}
}
}
