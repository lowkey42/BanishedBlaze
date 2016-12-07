/**************************************************************************\
 * terrain like transformable texture                                     *
 *                                               ___                      *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2016 Florian Oetke                                       *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include "texture.hpp"
#include "sprite_batch.hpp"
#include "material.hpp"

#include "../asset/asset_manager.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <vector>
#include <stdexcept>


namespace lux {
namespace graphic {

	class Smart_texture {
		public:
			enum class Point_location {
				none, on, between
			};

			Smart_texture(Material_ptr material={}, std::vector<glm::vec2> points={});

			auto material()const {return _material;}
			void material(Material_ptr material) {
				_material = std::move(material);
				_dirty = true;
			}
			auto shadowcaster()const {return _shadowcaster;}
			void shadowcaster(bool b) {
				_shadowcaster = b;
				_dirty = true;
			}
			auto decals_intensity()const {return _decals_intensity;}
			void decals_intensity(bool b) {
				_decals_intensity = b;
				_dirty = true;
			}

			auto points()const -> auto& {return _points;}
			void points(std::vector<glm::vec2> p) {_points=p; _dirty=true;}
			auto vertices()const -> std::vector<glm::vec2>;

			auto move_point(std::size_t i, glm::vec2 p) -> util::maybe<std::size_t>;
			void insert_point(std::size_t i, glm::vec2 p);
			void erase_point(std::size_t i);

			bool is_inside(glm::vec2 my_pos, glm::vec2 point)const;
			auto get_point(glm::vec2 my_pos,
			               glm::vec2 point,
			               float point_size)const -> std::tuple<Point_location,std::size_t>;

			void draw(glm::vec3 position, Sprite_batch&);

		private:
			Material_ptr _material;
			bool _shadowcaster=true;
			float _decals_intensity=0.f;
			std::vector<glm::vec2> _points;
			std::vector<Sprite_vertex> _vertices;
			bool _dirty;

			void _update_vertices();
	};

}
}
