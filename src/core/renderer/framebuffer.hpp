/** simple wrapper for OpenGL textures ***************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "texture.hpp"

#include "../utils/str_id.hpp"

#include <glm/vec3.hpp>

#include <string>
#include <unordered_map>
#include <tuple>


namespace lux {
namespace renderer {

	class Framebuffer {
		public:
			Framebuffer(int width, int height);
			Framebuffer(Framebuffer&&)noexcept;
			Framebuffer& operator=(Framebuffer&&)noexcept;
			~Framebuffer()noexcept;

			void add_depth_attachment();
			void add_depth_attachment(util::Str_id name);
			void add_color_attachment(util::Str_id name, int index, Texture_format format, bool linear_filtered=true);
			void build();

			void clear(glm::vec3 color=glm::vec3(0,0,0), bool depth=true);
			void clear_depth();
			void bind_target();
			void set_viewport();
			auto get_attachment(util::Str_id name)const -> const Texture& {
				auto it = _attachments.find(name);
				INVARIANT(it!=_attachments.end(), "Unknown attachment "<<name.str());
				return std::get<1>(it->second);
			}

		private:
			friend void debug_draw_framebuffer(Framebuffer&);

			using Attachment = std::tuple<int, Texture>;

			using Attachments = std::unordered_map<util::Str_id, Attachment>;

			uint32_t _handle;
			uint32_t _depth_buffer = 0;
			int _width, _height;
			Attachments _attachments;
			bool _depth_renderbuffer = false;
	};

	extern void bind_default_framebuffer();

} /* namespace renderer */
}
