#include "mesh.hpp"


namespace lux {
namespace graphic {

	namespace {
		template<typename T>
		auto read(asset::istream& stream) {
			T v;
			stream.read(reinterpret_cast<char*>(&v), sizeof(T));
			return v;
		}

		auto read_string(asset::istream& stream) {
			auto str_len = read<uint16_t>(stream);

			std::string str(str_len, ' ');
			if(str_len>0)
				stream.read(&str[0], static_cast<std::streamsize>(str.size()));

			return str;
		}
		auto read_material(asset::istream& stream) {
			auto aid = read_string(stream);
			return !aid.empty() ? stream.manager().load<Material>(asset::AID("mat"_strid, aid))
			                    : Material_ptr{};
		}

		std::vector<Sub_mesh_desc> load_mesh(asset::istream& in) {
			std::vector<Sub_mesh_desc> meshes;

			auto mesh_count = read<std::uint16_t>(in);
			meshes.reserve(mesh_count);

			for(auto i : util::range(mesh_count)) {
				(void)i;

				auto vertex_count = read<std::uint16_t>(in);
				auto index_count = read<std::uint16_t>(in);
				auto rigged = read<std::uint8_t>(in)!=0;
				auto material = read_material(in);

				meshes.emplace_back();
				auto& mesh = meshes.back();
				mesh.material = std::move(material);

				mesh.vertices.resize(vertex_count);
				in.read(reinterpret_cast<char*>(mesh.vertices.data()), sizeof(Mesh_vertex)*vertex_count);

				if(rigged) {
					mesh.vertices_rigged.resize(vertex_count);
					in.read(reinterpret_cast<char*>(mesh.vertices_rigged.data()),
					        sizeof(Mesh_vertex_rigged)*vertex_count);
				}

				mesh.indices.resize(index_count);
				in.read(reinterpret_cast<char*>(mesh.indices.data()), sizeof(std::uint16_t)*index_count);
			}

			return meshes;
		}
	}

	Vertex_layout mesh_vertex_layout {
		Vertex_layout::Mode::triangles,
		vertex("position",     &Mesh_vertex::position),
		vertex("uv",           &Mesh_vertex::uv),
		vertex("normal",       &Mesh_vertex::normal)
	};

	Vertex_layout mesh_rigged_vertex_layout {
		Vertex_layout::Mode::triangles,
		vertex("position",     &Mesh_vertex::position),
		vertex("uv",           &Mesh_vertex::uv),
		vertex("normal",       &Mesh_vertex::normal),
		vertex("bone_ids",     &Mesh_vertex_rigged::bone_ids, 1),
		vertex("bone_weights", &Mesh_vertex_rigged::bone_weights, 1)
	};


	Mesh::Mesh(asset::istream& in) : Mesh(load_mesh(in)) {}
	Mesh::Mesh(const std::vector<Sub_mesh_desc>& meshes) {
		reset(meshes);
	}

	void Mesh::draw(Command_queue& queue, Command base_command)const {
		for(auto& mesh : _meshes) {
			base_command.object(mesh.object);
			mesh.material->set_textures(base_command);
			queue.push_back(base_command);
		}
	}

	void Mesh::reset(const std::vector<Sub_mesh_desc>& meshes) {
		_meshes.clear();
		_meshes.reserve(meshes.size());

		if(meshes.empty())
			return;


		_rigged = not meshes.front().vertices_rigged.empty();

		for(auto& mesh : meshes) {
			INVARIANT(_rigged==(not mesh.vertices_rigged.empty()),
			          "All submeshes must be either rigged or not.");

			if(_rigged) {
				_meshes.emplace_back(Object{mesh_rigged_vertex_layout,
				                            create_buffer(mesh.vertices,        false, false),
				                            create_buffer(mesh.vertices_rigged, false, false),
				                            create_buffer(mesh.indices,         false, true)
				                     }, mesh.material);

			} else {
				_meshes.emplace_back(Object{mesh_vertex_layout,
				                            create_buffer(mesh.vertices, false, false),
				                            create_buffer(mesh.indices,  false, true)
				                     }, mesh.material);
			}
		}
	}

}
}
