/** System that orchastrates the rendering process ****************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "renderer_forward.hpp"
#include "renderer_lights.hpp"
#include "renderer_effects.hpp"

#include <core/graphic/command_queue.hpp>
#include <core/graphic/framebuffer.hpp>
#include <core/graphic/shader.hpp>
#include <core/graphic/skybox.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/uniform_buffer.hpp>

#include <core/units.hpp>


namespace lux {
class Engine;
namespace ecs {class Entity_manager;}
namespace graphic {
	class Camera;
	class Graphics_ctx;
}

namespace sys {
namespace renderer {
	
	struct Global_uniforms {
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 vp;
		glm::mat4 vp_inv;
		glm::mat4 sse_vp;
		glm::mat4 sse_vp_inv;
		glm::vec4 eye;
		float time;
	};
	
	
	class Renderer_system {
		public:
			Renderer_system(Engine& engine, ecs::Entity_manager&);
			
			void update(Time dt);
			void draw(const graphic::Camera&);
			
			void set_environment(const std::string& id);
			auto& lights() {return _light_renderer;}
			auto& effects() {return _effect_renderer;}
			
			void post_load();
			
		private:
			asset::Asset_manager&   _assets;
			graphic::Graphics_ctx&  _graphics_ctx;
			graphic::Command_queue  _render_queue;
			graphic::Uniform_buffer _global_uniforms;
			graphic::Shader_program _post_shader;

			graphic::Framebuffer    _canvas[2];
			graphic::Framebuffer    _canvas_no_depth[2];
			const graphic::Texture* _canvas_texture[2];
			bool                    _canvas_first_active = true;
			
			graphic::Framebuffer    _decals_canvas;
			
			Renderer_forward  _forward_renderer;
			Renderer_lights   _light_renderer;
			Renderer_effects  _effect_renderer;
			graphic::Skybox   _skybox;
			
			glm::vec3 _last_sse_cam_position;
			Time _time_acc {};
			
			
			auto _set_global_uniforms(const graphic::Camera& cam) -> Global_uniforms;
			void _draw_decals();
	};
	
}
}
}