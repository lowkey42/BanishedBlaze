/** batching texture renderer ************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "vertex_object.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "command_queue.hpp"

#include "../asset/aid.hpp"
#include "../units.hpp"

#include <vector>


namespace lux {
namespace graphic {

	struct Texture_Vertex {
		glm::vec2 position;
		glm::vec2 uv;
		const graphic::Texture* tex;

		Texture_Vertex(glm::vec2 pos, glm::vec2 uv_coords, const graphic::Texture* tex=nullptr);

		bool operator<(const Texture_Vertex& rhs)const noexcept {
			return tex < rhs.tex;
		}
	};

	extern Vertex_layout texture_batch_layout;

	extern void init_texture_renderer(asset::Asset_manager& asset_manager);

	extern void draw_fullscreen_quad(const graphic::Texture&, Texture_unit unit=Texture_unit::temporary);

	extern auto draw_texture(const graphic::Texture&, glm::vec2 pos,
	                         float scale=1.f) -> Command;

	extern auto quat_obj() -> Object&;

	class Texture_batch {
		public:
			Texture_batch(Shader_program& shader, std::size_t expected_size=64,
			              bool depth_test=false);
			Texture_batch(std::size_t expected_size=64,
			              bool depth_test=false);

			void insert(const Texture& texture, glm::vec2 pos, glm::vec2 size={-1,-1},
			            Angle rotation=Angle{0}, glm::vec4 clip_rect=glm::vec4{0,0,1,1});
			void flush(Command_queue&, bool order_independent=false);

			void layer(float layer) {_layer = layer;}

		private:
			using Vertex_citer = std::vector<Texture_Vertex>::const_iterator;
			
			Shader_program& _shader;
			
			std::vector<Texture_Vertex>   _vertices;
			std::vector<graphic::Object> _objects;
			std::size_t                   _free_obj = 0;
			bool _depth_test;

			float _layer = 0.f;

			void _draw(Command_queue&, bool order_independent);
			auto _draw_part(Vertex_citer begin, Vertex_citer end,
			                bool order_independent) -> Command;
			void _reserve_objects();
	};

}
}
