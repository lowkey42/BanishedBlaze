/** Renderer for post effects ************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "sprite_comp.hpp"
#include "terrain_comp.hpp"
#include "particle_comp.hpp"
#include "decal_comp.hpp"

#include "../../entity_events.hpp"

#include <core/graphic/graphics_ctx.hpp>
#include <core/graphic/command_queue.hpp>
#include <core/graphic/uniform_map.hpp>
#include <core/graphic/texture.hpp>
#include <core/graphic/framebuffer.hpp>
#include <core/graphic/texture_batch.hpp>
#include <core/graphic/primitives.hpp>
#include <core/utils/messagebus.hpp>


namespace lux {
namespace sys {
namespace renderer {

	class Renderer_effects {
		public:
			Renderer_effects(Engine& engine);

			void draw(graphic::Framebuffer& in);

			void motion_blur(glm::vec2 dir, float intensity) {
				_motion_blur_dir = dir;
				_motion_blur_intensity = intensity;
			}
			
		private:
			
			graphic::Graphics_ctx&  _graphics_ctx;
			graphic::Shader_program _blur_shader;
			graphic::Shader_program _motion_blur_shader;
			graphic::Shader_program _glow_shader;
			
			graphic::Framebuffer    _blur_canvas[2];
			const graphic::Texture* _blur_canvas_texture[2];
			graphic::Framebuffer    _motion_blur_canvas;
			const graphic::Texture* _motion_blur_canvas_texture;
			
			float _motion_blur_intensity = 0.f;
			glm::vec2 _motion_blur_dir;
	};

}
}
}
