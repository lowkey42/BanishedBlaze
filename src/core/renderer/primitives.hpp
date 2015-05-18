/**************************************************************************\
 * lines, circles, ...                                                    *
 *                                               ___                      *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2014 Florian Oetke                                       *
 *  Based on code of GDW-SS2014 project by Stefan Bodenschatz             *
 *  which was distributed under the MIT license.                          *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include "../units.hpp"
#include "shader.hpp"
#include "vertex_object.hpp"

namespace mo {
namespace renderer {

	class Ray_renderer {
		public:
			Ray_renderer(asset::Asset_manager& assets);

			void set_vp(const glm::mat4& vp);

			void draw(glm::vec3 p, Angle a, float length, float width=1);

		private:
			Shader_program _prog;
			Object _obj;
	};

}
}
