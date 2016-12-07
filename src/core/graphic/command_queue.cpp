#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "command_queue.hpp"

#include "texture.hpp"
#include "vertex_object.hpp"
#include "shader.hpp"

#include <glm/glm.hpp>

namespace lux {
namespace graphic {

	/*
	 * commands are sorted by _gl_options, _shader, _textures, _uniforms, _obj
	 * commands with order_dependent act as sync-points. Meaning all
	 *  operations up to this point must be applied before this one, but
	 *  later (non-order_dependent) commands may already be applied
	 */
	bool Command::operator<(const Command& rhs)const noexcept {
		return std::tie(_gl_options, _shader, _textures_hash, _ext_uniforms, _obj) <
			   std::tie(rhs._gl_options, rhs._shader, rhs._textures_hash, rhs._ext_uniforms, rhs._obj);
	}

	namespace {
		template<class T>
		auto hash_ptr_array(const T& array) {
			intptr_t hash = 0;
			for(auto ptr : array)
				hash = hash*31 + reinterpret_cast<intptr_t>(ptr);

			return hash;
		}
	}

	void Command::_update_hashes()noexcept {
		_textures_hash = static_cast<int>(hash_ptr_array(_textures));
	}

	Command& Command::shader(Shader_program& prog) {
		_shader = &prog;
		return *this;
	}
	Command& Command::texture(Texture_unit unit, const Texture& tex) {
		_textures.at(static_cast<std::size_t>(unit)) = &tex;
		return *this;
	}
	Command& Command::object(const Object& obj) {
		_obj = &obj;

		return *this;
	}
	Command& Command::order_dependent() {
		_order_dependent = 1;
		return *this;
	}
	Command& Command::order_independent() {
		_order_dependent = 0;
		return *this;
	}
	Command& Command::require(Gl_option opt) {
		_gl_options = _gl_options | opt;
		return *this;
	}

	Command& Command::require_not(Gl_option opt) {
		_gl_options = _gl_options & (~static_cast<Gl_options>(opt));
		return *this;
	}
	Command& Command::ext_uniforms(const IUniform_map& um) {
		_ext_uniforms = &um;
		return *this;
	}
	Cmd_uniform_map& Command::uniforms() {
		return _private_uniforms;
	}
	Command& Command::uniform_buffer(const Uniform_buffer* b, int slot) {
		for(auto& buffer : _uniform_buffers) {
			if(!std::get<1>(buffer)) {
				buffer = std::tie(slot, b);
				return *this;
			}
		}
		
		FAIL("No free slot in command for uniform buffer!");
	}


	Command_queue::Command_queue(std::size_t expected) {
		_commands.reserve(expected*0.9f);
		_order_dependent_commands.reserve(expected*0.1f);
	}


	namespace {
		void update_gl_options(Gl_options last, Gl_options next) {
			if((last&Gl_option::blend) != (next&Gl_option::blend)) {
				if(next&Gl_option::blend)
					glEnable(GL_BLEND);
				else
					glDisable(GL_BLEND);
			}

			if((last&Gl_option::depth_test) != (next&Gl_option::depth_test)) {
				if(next&Gl_option::depth_test)
					glEnable(GL_DEPTH_TEST);
				else
					glDisable(GL_DEPTH_TEST);
			}

			if((last&Gl_option::depth_write) != (next&Gl_option::depth_write)) {
				if(next&Gl_option::depth_write)
					glDepthMask(GL_TRUE);
				else
					glDepthMask(GL_FALSE);
			}
		}
	}

	void Command_queue::_execute_commands(const IUniform_map* shared_uniforms,
	                                      Command& last, bool& is_first,
	                                      std::vector<Command>& commands) {
		if(commands.empty())
			return;

		for(auto& cmd : commands) {
			// setup GL_options
			if(is_first || last._gl_options!=cmd._gl_options) {
				update_gl_options(last._gl_options, cmd._gl_options);
				last._gl_options = cmd._gl_options;
			}

			// setup shader
			bool shader_dirty = false;
			if(is_first || last._shader!=cmd._shader) {
				last._shader = cmd._shader;
				cmd._shader->bind();
				shader_dirty = true;
				if(shared_uniforms)
					shared_uniforms->bind_all(*cmd._shader);
			}

			if(cmd._ext_uniforms && (shader_dirty || last._ext_uniforms!=cmd._ext_uniforms)) {
				last._ext_uniforms = cmd._ext_uniforms;
				cmd._ext_uniforms->bind_all(*cmd._shader);
			}
			
			for(auto& ubo_binding : cmd._uniform_buffers) {
				if(std::get<1>(ubo_binding)) {
					std::get<1>(ubo_binding)->bind(std::get<0>(ubo_binding));
				}
			}

			cmd._private_uniforms.bind_all(*cmd._shader);

			// setup textures
			for(auto i=0u; i<texture_units; ++i) {
				if(cmd._textures[i] && (is_first || last._textures[i]!=cmd._textures[i])) {
					last._textures[i] = cmd._textures[i];
					cmd._textures[i]->bind(i);
				}
			}

			cmd._obj->draw();
			is_first = false;
		}
	}

	auto Command_queue::shared_uniforms(std::shared_ptr<IUniform_map> umap) -> IUniform_map& {
		_shared_uniforms = std::move(umap);
		return *shared_uniforms();
	}
	auto Command_queue::shared_uniforms() -> std::shared_ptr<IUniform_map> {
		INVARIANT(_shared_uniforms, "No uniforms set in this command queue");
		return _shared_uniforms;
	}

	bool Command_queue::flush() {
		if(_commands.empty() && _order_dependent_commands.empty())
			return false;

		std::sort(_commands.begin(), _commands.end());

		Command last;
		bool is_first = true;

		_execute_commands(_shared_uniforms.get(), last, is_first, _commands);
		_execute_commands(_shared_uniforms.get(), last, is_first, _order_dependent_commands);

		// reset GL_options if required
		update_gl_options(last._gl_options, default_gl_options);

		clear();
		return true;
	}

	void Command_queue::clear() {
		_commands.clear();
		_order_dependent_commands.clear();
	}

	void Command_queue::push_back(const Command& command) {
		std::vector<Command>& queue = command._order_dependent ?
		                                  _order_dependent_commands : _commands;

		queue.push_back(command);
		queue.back()._update_hashes();
	}

}
}
