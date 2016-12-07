/** simple wrapper for OpenGL textures ***************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "../utils/log.hpp"
#include "../asset/asset_manager.hpp"

namespace lux {
namespace graphic {

	struct Texture_loading_failed : public asset::Loading_failed {
		explicit Texture_loading_failed(const std::string& msg)noexcept;
	};

	enum Texture_format {
		RGB_565,
		DEPTH,    RG,     RGB,     RGBA,
		DEPTH_16, RG_16F, RGB_16F, RGBA_16F,
		DEPTH_24, RG_32F, RGB_32F, RGBA_32F
	};

	class Texture {
		public:
			explicit Texture(std::vector<uint8_t> buffer, bool cubemap);
			Texture(int width, int height, Texture_format format, const uint8_t* data=nullptr, bool linear_filtered=true);
			virtual ~Texture()noexcept;

			Texture& operator=(Texture&&)noexcept;
			Texture(Texture&&)noexcept;

			void bind(int index=0)const;

			auto clip_rect()const noexcept {return _clip;}

			auto width()const noexcept {return _width;}
			auto height()const noexcept {return _height;}

			auto unsafe_low_level_handle()const {
				return _handle;
			}

			Texture(const Texture&) = delete;
			Texture& operator=(const Texture&) = delete;

		protected:
			Texture(const Texture&, glm::vec4 clip)noexcept;
			void _update(const Texture&, glm::vec4 clip);

			unsigned int _handle;
			int          _width, _height;
			bool         _cubemap = false;
			bool         _owner = true;
			glm::vec4    _clip {0,0,1,1};
	};
	using Texture_ptr = asset::Ptr<Texture>;

	class Atlas_texture;

	class Texture_atlas : public std::enable_shared_from_this<Texture_atlas> {
		public:
			Texture_atlas(asset::istream);
			Texture_atlas& operator=(Texture_atlas&&)noexcept;
			~Texture_atlas();

			auto get(const std::string& name)const -> std::shared_ptr<Texture>;

		private:
			friend class Atlas_texture;
			Texture_ptr _texture;
			std::unordered_map<std::string, glm::vec4> _sub_positions;
			mutable std::unordered_map<std::string, std::weak_ptr<Atlas_texture>> _subs;
	};
	using Texture_atlas_ptr = asset::Ptr<Texture_atlas>;

} /* namespace renderer */


namespace asset {
	template<>
	struct Loader<graphic::Texture_atlas> {
		using RT = std::shared_ptr<graphic::Texture_atlas>;

		static RT load(istream in) {
			return std::make_shared<graphic::Texture_atlas>(std::move(in));
		}

		static void store(ostream, const graphic::Texture_atlas&) {
			FAIL("NOT IMPLEMENTED!");
		}
	};

	template<>
	struct Loader<graphic::Texture> {
		using RT = std::shared_ptr<graphic::Texture>;

		static RT load(istream in) {
			constexpr auto cube_aid = util::Str_id{"tex_cube"};
			return std::make_shared<graphic::Texture>(in.bytes(), in.aid().type()==cube_aid);
		}

		static void store(ostream, const graphic::Texture&) {
			FAIL("NOT IMPLEMENTED!");
		}
	};

	template<>
	struct Interceptor<graphic::Texture> {
		static auto on_intercept(Asset_manager& manager, const AID& interceptor_aid,
		                         const AID& org_aid) {
			auto atlas = manager.load<graphic::Texture_atlas>(interceptor_aid);

			return atlas->get(org_aid.name());
		}
	};
}

}
