#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "framebuffer.hpp"

#include <SDL2/SDL.h>

#include <glm/glm.hpp>


namespace lux {
namespace graphic {


	Framebuffer::Framebuffer(int width, int height)
		: _handle(0), _depth_buffer(0), _width(width), _height(height) {
	}

	Framebuffer::Framebuffer(Framebuffer&& rhs)noexcept
	    : _handle(rhs._handle),
	      _depth_buffer(rhs._depth_buffer),
	      _width(rhs._width),
	      _height(rhs._height),
	      _attachments(std::move(rhs._attachments)) {

		rhs._handle = 0;
		rhs._depth_buffer = 0;
		rhs._attachments.clear();
	}
	Framebuffer::~Framebuffer()noexcept {
		if(_handle)
			glDeleteFramebuffers(1, &_handle);

		if(_depth_buffer)
			glDeleteRenderbuffers(1, &_depth_buffer);

		_handle = 0;
		_depth_buffer = 0;
	}
	Framebuffer& Framebuffer::operator=(Framebuffer&& rhs)noexcept {
		if(_handle)
			glDeleteFramebuffers(1, &_handle);

		if(_depth_buffer)
			glDeleteRenderbuffers(1, &_depth_buffer);

		_handle = rhs._handle;
		_depth_buffer = rhs._depth_buffer;

		rhs._handle = 0;
		rhs._depth_buffer = 0;

		_width = rhs._width;
		_height = rhs._height;
		_attachments = std::move(rhs._attachments);

		return *this;
	}

	void Framebuffer::add_depth_attachment() {
		_depth_renderbuffer = true;
	}

	void Framebuffer::add_depth_attachment(util::Str_id name) {
		if(_depth_renderbuffer) {
			_depth_renderbuffer = false;
			WARN("rebound depth attachment");
		}

		auto new_texture = std::make_tuple(-1, Texture{_width, _height, Texture_format::DEPTH_24});

		auto res = _attachments.emplace(name, std::move(new_texture));
		if(!std::get<bool>(res)) {
			WARN("rebound named attachment "<<name.str());
			std::get<0>(res)->second = std::move(new_texture);
		}
	}

	void Framebuffer::add_color_attachment(util::Str_id name, int index, Texture_format format, bool linear_filtered) {
		auto new_texture = std::make_tuple(index, Texture{_width, _height, format, nullptr, linear_filtered});

		auto res = _attachments.emplace(name, std::move(new_texture));
		if(!std::get<bool>(res)) {
			WARN("rebound named attachment "<<name.str());
			std::get<0>(res)->second = std::move(new_texture);
		}
	}

	namespace {
		auto frambufferStatusToString(GLenum status) {
			switch(status) {
				case GL_FRAMEBUFFER_UNDEFINED:                     return "GL_FRAMEBUFFER_UNDEFINED";
				case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
				case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
				case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
				case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
				case GL_FRAMEBUFFER_UNSUPPORTED:                   return "GL_FRAMEBUFFER_UNSUPPORTED";
				case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:        return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
				case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:      return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
				default:                                           return "UNKNOWN";
			}
		}
	}

	void Framebuffer::build() {
		INVARIANT(!_attachments.empty(), "A framebuffer without any attachments makes no sense");

		if(!_handle) {
			glGenFramebuffers(1, &_handle);
		}

		bind_target();

		std::vector<GLuint> draw_buffers;
		draw_buffers.reserve(_attachments.size());

		for(const auto& attachment : _attachments) {
			auto&& index   = std::get<int>(attachment.second);
			auto&& texture = std::get<Texture>(attachment.second);

			texture.bind(0);
			auto attachment_idx = index==-1 ? GL_DEPTH_ATTACHMENT : GLenum(GL_COLOR_ATTACHMENT0+index);

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_idx,
			                       GL_TEXTURE_2D, texture.unsafe_low_level_handle(), 0);

			if(attachment_idx!=GL_DEPTH_ATTACHMENT) {
				draw_buffers.emplace_back(attachment_idx);
			}
		}

		if(!draw_buffers.empty())
			glDrawBuffers(int(draw_buffers.size()), draw_buffers.data());
		else
			glDrawBuffer(GL_NONE);

		if(_depth_renderbuffer) {
			glGenRenderbuffers(1, &_depth_buffer);
			glBindRenderbuffer(GL_RENDERBUFFER, _depth_buffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, _width, _height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			                          GL_RENDERBUFFER, _depth_buffer);
		}

		INVARIANT(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,
		          "Error constructing framebuffer: "<<frambufferStatusToString(glCheckFramebufferStatus(GL_FRAMEBUFFER)));

		bind_default_framebuffer();
	}


	void Framebuffer::set_viewport() {
		glViewport(0,0, _width, _height);
	}

	void Framebuffer::clear(glm::vec3 color, bool depth) {
		glClearColor(color.r, color.g, color.b, 0.f);
		glClear(GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0));
	}
	void Framebuffer::clear_depth() {
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void Framebuffer::bind_target() {
		INVARIANT(_handle!=0, "binding invalid framebuffer!");
		glBindFramebuffer(GL_FRAMEBUFFER, _handle);
	}
	void bind_default_framebuffer() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

} /* namespace renderer */
}
