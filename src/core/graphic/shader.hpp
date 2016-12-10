/** simple wrapper for OpenGL shader/programs ********************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "../asset/asset_manager.hpp"

#include <glm/glm.hpp>

#include <gsl.h>

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <tuple>


namespace lux {
namespace graphic {

	class Shader_program;

	struct Shader_compiler_error : public asset::Loading_failed {
		explicit Shader_compiler_error(const std::string& msg)noexcept : asset::Loading_failed(msg){}
	};

	enum class Shader_type {
		fragment,
		vertex
	};

	extern void preprocess_shader(Shader_type type, std::string& source, const std::string& name,
	                              asset::Asset_manager& assets);

	class Shader {
		public:
			Shader(Shader_type type, const std::string& source, const std::string& name);
			~Shader()noexcept;

			Shader& operator=(Shader&&);

		private:
			friend class Shader_program;
			unsigned int _handle;
			std::string _name;
			mutable std::vector<Shader_program*> _attached_to;

			void _on_attach(Shader_program* prog)const;
			void _on_detach(Shader_program* prog)const;
	};

	class Vertex_layout;

	struct IUniform_map;

	class Shader_program {
		public:
			Shader_program() = default;
			Shader_program(Shader_program&&)noexcept = default;
			Shader_program& operator=(Shader_program&&)noexcept = default;
			~Shader_program()noexcept;

			Shader_program& attach_shader(std::shared_ptr<const Shader> shader);
			Shader_program& bind_all_attribute_locations(const Vertex_layout&);
			Shader_program& bind_attribute_location(const std::string& name, int l);
			Shader_program& frag_data(unsigned int index, const std::string name);
			Shader_program& build();
			Shader_program& uniforms(std::unique_ptr<IUniform_map>&&);
			Shader_program& detach_all();
			Shader_program& uniform_buffer(const char* block_name, unsigned int slot);


			Shader_program& bind();
			Shader_program& unbind();
			
			Shader_program& set_uniform(const char* name, int value);
			Shader_program& set_uniform(const char* name, float value);
			Shader_program& set_uniform(const char* name, const glm::vec2& value);
			Shader_program& set_uniform(const char* name, const glm::vec3& value);
			Shader_program& set_uniform(const char* name, const glm::vec4& value);
			Shader_program& set_uniform(const char* name, const glm::mat2& value);
			Shader_program& set_uniform(const char* name, const glm::mat3& value);
			Shader_program& set_uniform(const char* name, const glm::mat4& value);

		private:
			struct Uniform_buffer_entry {
				unsigned int index=-1;
				unsigned int slot=0;
			};

			template<class T, class=void>
			struct Uniform_entry {
				int handle;
				T last_value;

				Uniform_entry() : handle(0) {}
				Uniform_entry(int handle, const T& value) : handle(handle), last_value(value) {}
				bool dirty(const T& v)const {return last_value!=v;}
				void set(const T& v) {last_value=v;}
			};
			template<class T>
			struct Uniform_entry<T, std::enable_if_t<sizeof(T)>=5*sizeof(float)>> {
				int handle;

				Uniform_entry() : handle(0) {}
				Uniform_entry(int handle, const T&) : handle(handle) {}
				bool dirty(const T&)const {return true;}
				void set(const T&) {}
			};
			template<class T>
			using Uniform_cache = std::unordered_map<std::string, Uniform_entry<T>>;

			struct Prog_handle {
				unsigned int v;
				Prog_handle();
				Prog_handle(Prog_handle&&)noexcept;
				Prog_handle& operator=(Prog_handle&&)noexcept;
				~Prog_handle();
				operator unsigned int()const noexcept;
			};

			Prog_handle _handle;
			std::vector<std::shared_ptr<const Shader>> _attached_shaders;
			std::vector<std::tuple<unsigned int, std::string>> _frag_data_locations;
			std::unordered_map<std::string, Uniform_buffer_entry>  _uniform_buffer_locations;
			std::unique_ptr<IUniform_map> _uniforms;

			Uniform_cache<int>       _uniform_locations_int;
			Uniform_cache<float>     _uniform_locations_float;
			Uniform_cache<glm::vec2> _uniform_locations_vec2;
			Uniform_cache<glm::vec3> _uniform_locations_vec3;
			Uniform_cache<glm::vec4> _uniform_locations_vec4;
			Uniform_cache<glm::mat2> _uniform_locations_mat2;
			Uniform_cache<glm::mat3> _uniform_locations_mat3;
			Uniform_cache<glm::mat4> _uniform_locations_mat4;
	};

} /* namespace renderer */

namespace asset {
	template<>
	struct Loader<graphic::Shader> {
		using RT = std::shared_ptr<graphic::Shader>;

		static RT load(istream in) {
			auto shader_type = graphic::Shader_type::fragment;

			switch(in.aid().type()) {
				case "frag_shader"_strid:
					shader_type = graphic::Shader_type::fragment;
					break;

				case "vert_shader"_strid:
					shader_type = graphic::Shader_type::vertex;
					break;

				default:
					throw Loading_failed("Unsupported assetId for shader: "+in.aid().str());
			}

			auto src = in.content();
			preprocess_shader(shader_type, src, in.aid().str(), in.manager());
			return std::make_shared<graphic::Shader>(shader_type, src, in.aid().str());
		}

		static void store(istream, const graphic::Shader&) {
			throw Loading_failed("Saving shaders is not supported!");
		}
	};
}
}
