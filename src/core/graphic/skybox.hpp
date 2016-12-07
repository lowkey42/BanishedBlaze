/** Renders a simple skybox **************************************************
 *                                                                           *
 * Copyright (c) 2015 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "texture.hpp"
#include "shader.hpp"
#include "vertex_object.hpp"

#include "command_queue.hpp"

namespace lux {
namespace graphic {

	class Skybox {
		public:
			Skybox(asset::Asset_manager&);

			void draw(Command_queue&)const;

			void texture(Texture_ptr tex);

			void tint(glm::vec3 t){_tint = t;_dirty=true;}
			void brightness(float b){_brightness = b; _dirty=true;}

		private:
			mutable Shader_program _prog;
			Object         _obj;
			Texture_ptr    _tex;
			glm::vec3      _tint {1.f,1.f,1.f};
			float          _brightness = 1.f;
			mutable bool   _dirty = true;
	};

}
}
