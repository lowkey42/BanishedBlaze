/** A renderable 3D object ***************************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "texture.hpp"
#include "vertex_object.hpp"
#include "command_queue.hpp"
#include "material.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace lux {
namespace graphic {

	struct Mesh_vertex {
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
	};
	static_assert(sizeof(Mesh_vertex)==sizeof(float)*(3+2+3), "Padding is not allowed!");

	struct Mesh_vertex_rigged {
		glm::vec4 bone_ids;
		glm::vec4 bone_weights;
	};
	static_assert(sizeof(Mesh_vertex_rigged)==sizeof(float)*(4+4), "Padding is not allowed!");

	extern Vertex_layout mesh_vertex_layout;
	extern Vertex_layout mesh_rigged_vertex_layout;

	struct Sub_mesh_desc {
		std::vector<Mesh_vertex>        vertices;
		std::vector<Mesh_vertex_rigged> vertices_rigged;
		std::vector<std::uint16_t> indices;
		Material_ptr material;
	};


	class Mesh {
		public:
			Mesh(asset::istream&);
			Mesh(const std::vector<Sub_mesh_desc>& meshes);

			void draw(Command_queue&, Command base_command)const;

			void reset(const std::vector<Sub_mesh_desc>& meshes);

			auto rigged()const noexcept {return _rigged;}

		private:
			struct Sub_mesh {
				Object       object;
				Material_ptr material;

				Sub_mesh() = default;
				Sub_mesh(Object object, Material_ptr material)
				    : object(std::move(object)), material(std::move(material)) {}
			};

			std::vector<Sub_mesh> _meshes;
			bool _rigged;
	};

}

namespace asset {
	template<>
	struct Loader<graphic::Mesh> {
		using RT = std::shared_ptr<graphic::Mesh>;

		static RT load(istream in) {
			return std::make_shared<graphic::Mesh>(in);
		}

		static void store(ostream, const graphic::Mesh&) {
			FAIL("NOT IMPLEMENTED, YET!");
		}
	};
}
}
