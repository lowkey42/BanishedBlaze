#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "shader.hpp"

#include "vertex_object.hpp"

#include "uniform_map.hpp"

#include "../utils/string_utils.hpp"
#include "../utils/log.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <regex>


namespace lux {
namespace graphic {

	namespace {
		GLenum shader_type_to_GLenum(Shader_type type) {
			switch(type) {
				case Shader_type::vertex:   return GL_VERTEX_SHADER;
				case Shader_type::fragment: return GL_FRAGMENT_SHADER;
				default: FAIL("Unsupported ShaderType");
				return 0;
			}
		}

		util::maybe<const std::string> read_gl_info_log(unsigned int handle) {
			auto info_log_length=0;
			glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &info_log_length);
			if( info_log_length>1 ) {
				auto info_log_buffer = std::string(info_log_length, ' ');

				glGetShaderInfoLog(handle, info_log_length, NULL, &info_log_buffer[0]);
				return std::move(info_log_buffer);
			};

			return util::nothing();
		}

		util::maybe<const std::string> read_gl_prog_info_log(unsigned int handle) {
			auto info_log_length=0;
			glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &info_log_length);
			if( info_log_length>1 ) {
				auto info_log_buffer = std::string(info_log_length, ' ');

				glGetProgramInfoLog(handle, info_log_length, NULL, &info_log_buffer[0]);
				return std::move(info_log_buffer);
			};

			return util::nothing();
		}

		bool get_gl_proc_status(unsigned int handle, GLenum status_type) {
			GLint success = 0;
			glGetProgramiv(handle, status_type, &success);

			return success!=0;
		}
		bool get_gl_shader_status(unsigned int handle, GLenum status_type) {
			GLint success = 0;
			glGetShaderiv(handle, status_type, &success);

			return success!=0;
		}
	}


	void preprocess_shader(Shader_type type, std::string& source, const std::string& name,
	                       asset::Asset_manager& assets) {
		static const std::regex include_regex("#include <([^>]*)>");

		std::smatch match;
		while(std::regex_search(source, match, include_regex)) {
			auto included_name = std::string(match[1]);

			auto in = assets.load_raw(asset::AID{"shader"_strid, included_name});
			INVARIANT(in.is_some(), "Couldn't find shader '"<<included_name<<"' included in '"<<name<<"'");

			auto included_src = in.get_or_throw().content();
			auto begin = source.begin()+match.position();
			source.replace(begin, begin+match.length(),
			               included_src.begin(), included_src.end());
		}


		static const std::regex skip_regex("#skip begin\n(.|\n)*?#skip end\n");

		while(std::regex_search(source, match, skip_regex)) {
			source.erase(match.position(), match.length());
		}
		
#ifdef EMSCRIPTEN
		util::replace_inplace(source, "#version auto", "#version 300 es");
#else
		util::replace_inplace(source, "#version auto", "#version 330 core");
#endif
	}

	Shader::Shader(Shader_type type, const std::string& source, const std::string& name)
	    : _name(name) {
		char const * source_pointer = source.c_str();
		int len = source.length();

		_handle = glCreateShader(shader_type_to_GLenum(type));
		glShaderSource(_handle, 1, &source_pointer , &len);
		glCompileShader(_handle);


		bool success = get_gl_shader_status(_handle, GL_COMPILE_STATUS);
		auto& log = success ? util::info(__func__, __FILE__, __LINE__)
		                    : util::warn(__func__, __FILE__, __LINE__);

		log<<"Compiling shader "<<_handle<<":"<<name;
		read_gl_info_log(_handle).process([&](const auto& _){
			log<<"\n"<<_;
		});
		log<<std::endl;

		if(!success)
			throw Shader_compiler_error("Shader compiler failed for \""+name+"\"");
	}
	Shader::~Shader()noexcept {
		if(_handle!=0)
			glDeleteShader(_handle);
	}

	void Shader::_on_attach(Shader_program* prog)const {
		_attached_to.push_back(prog);
	}
	void Shader::_on_detach(Shader_program* prog)const {
		_attached_to.erase(std::remove(_attached_to.begin(), _attached_to.end(), prog), _attached_to.end());
	}
	Shader& Shader::operator=(Shader&& s) {
		if(_handle!=0)
			glDeleteShader(_handle);

		_handle = s._handle;
		_name = std::move(s._name);
		s._handle = 0;

		for(auto prog : _attached_to)
			prog->build();

		return *this;
	}

	Shader_program::Prog_handle::Prog_handle() {
		v = glCreateProgram();
	}
	Shader_program::Prog_handle::Prog_handle(Prog_handle&& rhs)noexcept : v(rhs.v) {
		rhs.v = 0;
	}
	auto Shader_program::Prog_handle::operator=(Prog_handle&& rhs)noexcept -> Prog_handle& {
		v = rhs.v;
		rhs.v = 0;
		return *this;
	}
	Shader_program::Prog_handle::~Prog_handle() {
		if(v!=0) {
			glDeleteProgram(v);
		}
	}
	Shader_program::Prog_handle::operator unsigned int()const noexcept {
		INVARIANT(v!=0, "Access to moved from shader_program");
		return v;
	}

	Shader_program::~Shader_program()noexcept {
		detach_all();
	}

	Shader_program& Shader_program::attach_shader(std::shared_ptr<const Shader> shader) {
		shader->_on_attach(this);
		_attached_shaders.emplace_back(shader);
		return *this;
	}
	Shader_program& Shader_program::detach_all() {
		for(auto& s : _attached_shaders)
			s->_on_detach(this);

		_attached_shaders.clear();
		return *this;
	}

	Shader_program& Shader_program::frag_data(unsigned int index, const std::string name) {
		_frag_data_locations.emplace_back(index, name);
		return *this;
	}

	Shader_program& Shader_program::build() {
		for(auto& s : _attached_shaders)
			glAttachShader(_handle, s->_handle);

		for(auto& frag_data : _frag_data_locations) {
			glBindFragDataLocation(_handle, std::get<0>(frag_data), std::get<1>(frag_data).c_str());
		}

		glLinkProgram(_handle);

		bool success = get_gl_proc_status(_handle, GL_LINK_STATUS);

		auto& log = success ? util::info(__func__, __FILE__, __LINE__)
		                    : util::warn(__func__, __FILE__, __LINE__);

		log<<"Linking shaders "<<_handle<<": ";
		for(auto& s : _attached_shaders)
			log<<s->_name<<" ";

		read_gl_prog_info_log(_handle).process([&](const auto& _){
			log<<"\n"<<_;
		});

		glValidateProgram(_handle);
		read_gl_prog_info_log(_handle).process([&](const auto& _){
			log<<"\nValidation log: "<<_;
		});
		log<<std::endl;

		if(!success)
			throw Shader_compiler_error("Shader linker failed");

		for(auto& s : _attached_shaders)
			glDetachShader(_handle, s->_handle);

		// clear uniform caches
		_uniform_locations_int.clear();
		_uniform_locations_float.clear();
		_uniform_locations_vec2.clear();
		_uniform_locations_vec3.clear();
		_uniform_locations_vec4.clear();
		_uniform_locations_mat2.clear();
		_uniform_locations_mat3.clear();
		_uniform_locations_mat4.clear();

		if(_uniforms) {
			glUseProgram(_handle);
			_uniforms->bind_all(*this);
			glUseProgram(0);
		}
		
		for(auto& uniform_buffer : _uniform_buffer_locations) {
			glUniformBlockBinding(_handle,
			                      uniform_buffer.second.index,
			                      uniform_buffer.second.slot);
		}

		return *this;
	}

	Shader_program& Shader_program::uniforms(std::unique_ptr<IUniform_map>&& uniforms) {
		_uniforms = std::move(uniforms);
		if(_uniforms) {
			glUseProgram(_handle);
			_uniforms->bind_all(*this);
		}

		return *this;
	}
	
	Shader_program& Shader_program::uniform_buffer(const char* block_name, int slot) {
		auto& block = _uniform_buffer_locations[block_name];
		if(block.index==0 || block.slot!=slot) {
			block.slot = slot;
			
			if(block.index==0)
				block.index = glGetUniformBlockIndex(_handle, block_name);
			
			glUniformBlockBinding(_handle, block.index, slot);
		}
		
		return *this;
	}

	Shader_program& Shader_program::bind_all_attribute_locations(const Vertex_layout& vl) {
		vl.setup_shader(*this);
		return *this;
	}
	Shader_program& Shader_program::bind_attribute_location(const std::string& name, int l) {
		glBindAttribLocation(_handle, l, name.c_str());
		return *this;
	}


	Shader_program& Shader_program::bind() {
		glUseProgram(_handle);

		return *this;
	}
	Shader_program& Shader_program::unbind() {
		glUseProgram(0);

		return *this;
	}


	namespace {
		template<class T, class V>
		auto locate_uniform(const char* name, int shader_handle, T& cache, const V& value) {
			using ET = typename T::mapped_type;

			auto dirty = false;
			auto it = cache.find(name);
			if (it == cache.end()) {
				it = cache.emplace(name,
				                   ET{glGetUniformLocation(shader_handle, name), value}).first;
				dirty = true;
			} else if(it->second.dirty(value)) {
				dirty = true;
				it->second.set(value);
			}

			return std::make_pair(dirty, it->second.handle);
		}
	}

	Shader_program& Shader_program::set_uniform(const char* name, int value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_int, value);

		if(dirty)
			glUniform1i(handle, value);

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, float value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_float, value);

		if(dirty)
			glUniform1f(handle, value);

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::vec2& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_vec2, value);

		if(dirty)
			glUniform2fv(handle, 1, glm::value_ptr(value));

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::vec3& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_vec3, value);

		if(dirty)
			glUniform3fv(handle, 1, glm::value_ptr(value));

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::vec4& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_vec4, value);

		if(dirty)
			glUniform4fv(handle, 1, glm::value_ptr(value));

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::mat2& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_mat2, value);

		if(dirty)
			glUniformMatrix2fv(handle, 1, GL_FALSE, glm::value_ptr(value));

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::mat3& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_mat3, value);

		if(dirty)
			glUniformMatrix3fv(handle, 1, GL_FALSE, glm::value_ptr(value));

		return *this;
	}
	Shader_program& Shader_program::set_uniform(const char* name, const glm::mat4& value) {
		auto dirty = true;
		auto handle = 0;
		std::tie(dirty, handle) = locate_uniform(name, _handle, _uniform_locations_mat4, value);

		if(dirty)
			glUniformMatrix4fv(handle, 1, GL_FALSE, glm::value_ptr(value));

		return *this;
	}

} /* namespace renderer */
}
