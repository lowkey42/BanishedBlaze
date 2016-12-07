#ifndef ANDROID
	#include <GL/glew.h>
	#include <GL/gl.h>
#else
	#include <GLES2/gl2.h>
#endif

#include "texture.hpp"

#include <SDL2/SDL.h>
#include <soil/SOIL2.h>
#include <glm/glm.hpp>


namespace lux {
namespace graphic {

	using namespace glm;

	Texture_loading_failed::Texture_loading_failed(const std::string& msg)noexcept : Loading_failed(msg){}


#define CLAMP_TO_EDGE 0x812F

	Texture::Texture(std::vector<uint8_t> buffer, bool cubemap)
	    : _cubemap(cubemap) {

		if(!cubemap) {
			_handle = SOIL_load_OGL_texture_from_memory
			(
				buffer.data(),
				int(buffer.size()),
				SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID,
				0,
				&_width,
				&_height
			);
		} else {
			_handle = SOIL_load_OGL_single_cubemap_from_memory
			(
				buffer.data(),
				int(buffer.size()),
				SOIL_DDS_CUBEMAP_FACE_ORDER,
				SOIL_LOAD_AUTO,
				SOIL_CREATE_NEW_ID,
				SOIL_FLAG_MIPMAPS
			);
		}

		if(!_handle)
			throw Texture_loading_failed(SOIL_last_result());

		auto tex_type = GLenum(_cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D);

		bind(0);
		glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER, _cubemap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, CLAMP_TO_EDGE);
		glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, CLAMP_TO_EDGE);
		glBindTexture(tex_type, 0);
	}
	Texture::Texture(int width, int height, Texture_format format, const uint8_t* data, bool linear_filtered)
	    : _width(width), _height(height) {

		glGenTextures(1, &_handle);
		glBindTexture(GL_TEXTURE_2D, _handle);

		GLint gl_internal_format;
		GLenum gl_format;
		GLenum gl_type;

		switch(format) {
			case Texture_format::DEPTH:
				gl_internal_format = GL_DEPTH_COMPONENT;
				gl_format = GL_DEPTH_COMPONENT;
				gl_type = GL_UNSIGNED_BYTE;
				break;
			case Texture_format::DEPTH_16:
				gl_internal_format = GL_DEPTH_COMPONENT16;
				gl_format = GL_DEPTH_COMPONENT;
				gl_type = GL_UNSIGNED_BYTE;
				break;
			case Texture_format::DEPTH_24:
				gl_internal_format = GL_DEPTH_COMPONENT24;
				gl_format = GL_DEPTH_COMPONENT;
				gl_type = GL_UNSIGNED_BYTE;
				break;

			case Texture_format::RG:
				gl_internal_format = GL_RG;
				gl_format = GL_RG;
				gl_type = GL_UNSIGNED_BYTE;
				break;
			case Texture_format::RG_16F:
				gl_internal_format = GL_RG16F;
				gl_format = GL_RG;
				gl_type = GL_FLOAT;
				break;
			case Texture_format::RG_32F:
				gl_internal_format = GL_RG32F;
				gl_format = GL_RG;
				gl_type = GL_FLOAT;
				break;

			case Texture_format::RGB_565:
				gl_internal_format = GL_RGB565;
				gl_format = GL_RGB;
				gl_type = GL_UNSIGNED_BYTE;
				break;
				
			case Texture_format::RGB:
				gl_internal_format = GL_RGB;
				gl_format = GL_RGB;
				gl_type = GL_UNSIGNED_BYTE;
				break;
			case Texture_format::RGB_16F:
				gl_internal_format = GL_RGB16F;
				gl_format = GL_RGB;
				gl_type = GL_FLOAT;
				break;
			case Texture_format::RGB_32F:
				gl_internal_format = GL_RGB32F;
				gl_format = GL_RGB;
				gl_type = GL_FLOAT;
				break;

			case Texture_format::RGBA:
				gl_internal_format = GL_RGBA;
				gl_format = GL_RGBA;
				gl_type = GL_UNSIGNED_BYTE;
				break;
			case Texture_format::RGBA_16F:
				gl_internal_format = GL_RGBA16F;
				gl_format = GL_RGBA;
				gl_type = GL_FLOAT;
				break;
			case Texture_format::RGBA_32F:
				gl_internal_format = GL_RGBA32F;
				gl_format = GL_RGBA;
				gl_type = GL_FLOAT;
				break;
		}

		glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, _width, _height, 0, gl_format, gl_type, data);

		if(linear_filtered) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, CLAMP_TO_EDGE);
	}

	Texture::Texture(const Texture& base, glm::vec4 clip)noexcept
	    : _handle(base._handle),
	      _width (int(base._width  * (clip.z-clip.x))),
	      _height(int(base._height * (clip.w-clip.y))),
	      _owner(false),
	      _clip(clip) {
	}

	void Texture::_update(const Texture& base, glm::vec4 clip) {
		INVARIANT(!_owner, "_update is only supported for non-owning textures!");

		_handle = base._handle;
		_width  = int(base._width * (clip.z-clip.x));
		_height = int(base._height * (clip.w-clip.y));
		_clip = clip;
	}

	Texture::~Texture()noexcept {
		if(_handle!=0 && _owner) {
			glDeleteTextures(1, &_handle);
		}
	}

	Texture::Texture(Texture&& rhs)noexcept
	    : _handle(rhs._handle), _width(rhs._width),
	      _height(rhs._height), _cubemap(rhs._cubemap), _owner(rhs._owner), _clip(rhs._clip) {

		rhs._handle = 0;
	}
	Texture& Texture::operator=(Texture&& s)noexcept {
		if(_handle!=0 && _owner)
			glDeleteTextures(1, &_handle);

		_cubemap = s._cubemap;
		_owner = s._owner;
		_handle = s._handle;
		s._handle = 0;

		_width = s._width;
		_height = s._height;
		_clip = s._clip;

		return *this;
	}

	void Texture::bind(int index)const {
		auto tex = GL_TEXTURE0+index;
		INVARIANT(tex<GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, "to many textures");

		glActiveTexture(GLenum(GL_TEXTURE0 + index));
		glBindTexture(_cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, _handle);
	}



	class Atlas_texture : public Texture {
		public:
			Atlas_texture(const std::string& name,
			              std::shared_ptr<const Texture_atlas>, glm::vec4 clip);
			~Atlas_texture();

			using Texture::_update;

		private:
			const std::string _name;
			std::shared_ptr<const Texture_atlas> _atlas;
	};

	namespace {
		struct Frame {
			std::string name;
			float x, y, width, height;
		};
		sf2_structDef(Frame, name,x,y,width,height)

		struct Atlas_def {
			std::string texture;
			std::vector<Frame> frames;
		};
		sf2_structDef(Atlas_def, texture,frames)
	}

	Texture_atlas::Texture_atlas(asset::istream stream) {
		Atlas_def def;
		sf2::deserialize_json(stream, [&](auto& msg, uint32_t row, uint32_t column) {
			ERROR("Error parsing TextureAtlas from "<<stream.aid().str()<<" at "<<row<<":"<<column<<": "<<msg);
		}, def);

		_texture = stream.manager().load<Texture>(asset::AID(def.texture));
		_sub_positions.reserve(def.frames.size());
		_subs.reserve(def.frames.size());

		auto tex_size = vec2{_texture->width(), _texture->height()};

		for(Frame& frame : def.frames) {
			auto clip = vec4 {
				frame.x/tex_size.x,
				frame.y/tex_size.y,
				(frame.x+frame.width)/tex_size.x,
				(frame.y+frame.height)/tex_size.y
			};
			_sub_positions.emplace(frame.name, clip);
		}
	}
	Texture_atlas& Texture_atlas::operator=(Texture_atlas&& rhs)noexcept {
		_texture = std::move(rhs._texture);
		_sub_positions = std::move(rhs._sub_positions);

		for(auto& sub : _subs) {
			auto sub_tex = sub.second.lock();
			INVARIANT(sub_tex, "Texture has not been removed from its atlas");

			auto iter = _sub_positions.find(sub.first);
			if(iter==_sub_positions.end()) {
				ERROR("Texture that has been removed from atlas is still in use!");
				sub_tex->_update(*_texture, sub_tex->clip_rect());
				continue;
			}

			sub_tex->_update(*_texture, iter->second);
		}

		return *this;
	}
	Texture_atlas::~Texture_atlas() {
		INVARIANT(_subs.empty(), "A texture from this atlas has not been deleted");
	}
	auto Texture_atlas::get(const std::string& name)const -> std::shared_ptr<Texture> {
		auto& weak_sub = _subs[name];
		auto sub = weak_sub.lock();

		if(sub) {
			INVARIANT(sub, "Texture has not been removed from its atlas");
			return sub;
		}

		auto iter = _sub_positions.find(name);
		if(iter==_sub_positions.end())
			throw Texture_loading_failed("Couldn't find texture \""+name+"\" in atlas \""+_texture.aid().str()+"\"");

		sub = std::make_shared<Atlas_texture>(name, shared_from_this(), iter->second);
		weak_sub = sub;
		return sub;
	}

	Atlas_texture::Atlas_texture(const std::string& name,
	                             std::shared_ptr<const Texture_atlas> atlas, glm::vec4 clip)
	    : Texture(*atlas->_texture, clip), _name(name), _atlas(atlas) {
	}

	Atlas_texture::~Atlas_texture() {
		if(_atlas)
			_atlas->_subs.erase(_name);
	}

} /* namespace renderer */
}
