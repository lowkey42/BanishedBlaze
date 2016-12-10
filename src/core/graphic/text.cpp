#define GLM_SWIZZLE

#include "text.hpp"

#include "shader.hpp"
#include "command_queue.hpp"

#include "../asset/asset_manager.hpp"

#include <glm/glm.hpp>

#include <glm/gtx/rotate_vector.hpp>


namespace lux {
namespace graphic {

	Font::Font(asset::Asset_manager& assets, std::istream& stream) {
		std::string line;
		std::getline(stream, line);

		int size=0;
		stream>>size;
		stream>>_height;
		_line_height = _height;

		std::getline(stream, line);
		std::getline(stream, line);
		util::replace_inplace(line, "\r", "");
		_texture = assets.load<Texture>(asset::AID("tex"_strid, line));

		int symbols;
		stream>>symbols;

		_glyphs.reserve(symbols);
		for(auto i : util::range(symbols)) {
			Text_char id = i;
			stream>>id;

			Glyph& g = _glyphs[id];
			stream>> g.x >> g.y >> g.width >> g.height >> g.offset_x >> g.offset_y >> g.advance;
		}

		if(stream.good()) {
			int kernings = 0;
			stream>> kernings;

			for(auto i : util::range(kernings)) {
				Text_char id=i, prev;
				int val=0;
				stream>> id >> prev >> val;

				_glyphs[id].kerning[prev] = val;
			}
		}
	}

	using glm::vec2;

	namespace {
		// FIXME: must use Texute::clip() to calculate scale and UVs
		template<class F>
		void create_quad(const F& mod_clip, std::vector<Simple_vertex>& vertices,
		                float x, float y,
		                float u, float v,
		                float w, float h,
		                float tw,float th) {
			vertices.push_back({{x  ,y  },   mod_clip({u    /tw, (v  )/th})});
			vertices.push_back({{x  ,(y+h)}, mod_clip({u    /tw, (v+h)/th})});
			vertices.push_back({{x+w,y  },   mod_clip({(u+w)/tw, (v  )/th})});

			vertices.push_back({{x+w,(y+h)}, mod_clip({(u+w)/tw, (v+h)/th})});
			vertices.push_back({{x+w,y  },   mod_clip({(u+w)/tw, (v  )/th})});
			vertices.push_back({{x  ,(y+h)}, mod_clip({u    /tw, (v+h)/th})});
		}

		template<typename Func>
		void parse(const std::string str, int height, int tex_width, int tex_height,
		           const std::unordered_map<Text_char, Glyph>& glyphs, Func quad_callback,
		           bool monospace=false) {

			glm::vec2 offset{0,-height};
			Text_char prev = 0;

			auto tw = tex_width;
			auto th = tex_height;

			auto max_advance = 0.f;
			auto sum_advance = 0.f;
			if(monospace) {
				for(auto& g : glyphs) {
					max_advance = std::max(static_cast<float>(g.second.advance), max_advance);
					sum_advance += g.second.advance;
				}
			}
			float fixed_advance = max_advance*0.5f + sum_advance/glyphs.size()*0.5f;

			auto add_glyph = [&](Text_char c) {
				if(c=='\n') {
					offset.x=0;
					offset.y+=height;
					return;
				}

				auto g = glyphs.find(c);
				if(g==glyphs.end())
					g=glyphs.begin();

				auto& glyph = g->second;

				auto k = glyph.kerning.find(prev);
				if(!monospace && k!=glyph.kerning.end())
					offset.x+=k->second;

				quad_callback(
				            offset.x + glyph.offset_x,
				            offset.y + glyph.offset_y,
				            glyph.x,
				            glyph.y,
				            glyph.width,
				            glyph.height,
				            tw, th);


				if(fixed_advance>0.f) {
					offset.x+=fixed_advance;
				} else {
					offset.x+= glyph.advance;
				}

				prev = c;
			};

			for(auto iter=str.begin(); iter!=str.end(); ++iter) {
				auto b0 = *iter >> 7;

				if(b0==0)
					add_glyph(*iter);

				else {
					auto b1 = *iter >>6 & 1;
					auto b2 = *iter >>5 & 1;
					auto b3 = *iter >>4 & 1;

					if(b1==0) { // 1 byte
						add_glyph(*iter);

					} else if(b2==0) { // 2 byte
						auto byte_1 = (*iter & 0b00011111);
						iter++;
						auto byte_2 = (*iter & 0b00111111);
						add_glyph(byte_1<<6 | byte_2);

					} else if(b3==0) { // 3 byte
						auto byte_1 = (*iter & 0b00011111);
						iter++;
						auto byte_2 = (*iter & 0b00111111);
						iter++;
						auto byte_3 = (*iter & 0b00111111);
						add_glyph((byte_1<<6 | byte_2)<<6 | byte_3);

					} else { // 4 byte
						auto byte_1 = (*iter & 0b00011111);
						iter++;
						auto byte_2 = (*iter & 0b00111111);
						iter++;
						auto byte_3 = (*iter & 0b00111111);
						iter++;
						auto byte_4 = (*iter & 0b00111111);
						add_glyph(((byte_1<<6 | byte_2)<<6 | byte_3)<<6 | byte_4);
					}
				}

			}
		}
	}

	void Font::calculate_vertices(const std::string& str,
	                              std::vector<Simple_vertex>& vertices, bool monospace)const {

		vertices.reserve(str.length()*4);

		auto tex_clip_size = _texture->clip_rect().zw() - _texture->clip_rect().xy();
		auto mod_clip = [&](glm::vec2 uv) {
			return _texture->clip_rect().xy() + uv*tex_clip_size;
		};

		parse(str, _height, _texture->width(), _texture->height(), _glyphs, [&](auto... args) {
			create_quad(mod_clip, vertices, args...);
		}, monospace);
	}

	auto Font::text(const std::string& str)const -> Text_ptr {
		auto& entry =_cache[str];

		if(entry)
			return entry;

		std::vector<Simple_vertex> vertices;
		calculate_vertices(str, vertices);

		return entry=std::make_shared<Text>(shared_from_this(), vertices);
	}

	auto Font::calculate_size(const std::string& str)const -> glm::vec2 {
		glm::vec2 top_left, bottom_right;

		parse(str, _height, _texture->width(), _texture->height(), _glyphs, [&](
		      float x, float y,
		      float u, float v,
              float w, float h,
              float tw,float th) {
			if(x<top_left.x) top_left.x=x;
			if(y<top_left.y) top_left.y=y;

			if(x+w>bottom_right.x) bottom_right.x=x;
			if(y+h>bottom_right.y) bottom_right.y=y;
		});

		return bottom_right - top_left;
	}


	Text::Text(Font_sptr font, std::vector<Simple_vertex> vertices)
	    : _font(font), _obj(simple_vertex_layout, create_buffer(vertices)) {

		glm::vec2 top_left, bottom_right;
		for(auto& v : vertices) {
			if(v.xy.x<top_left.x) top_left.x=v.xy.x;
			if(v.xy.y<top_left.y) top_left.y=v.xy.y;

			if(v.xy.x>bottom_right.x) bottom_right.x=v.xy.x;
			if(v.xy.y>bottom_right.y) bottom_right.y=v.xy.y;
		}

		_size = bottom_right - top_left;
	}

	Text_dynamic::Text_dynamic(Font_sptr font)
	    : _font(font),
	      _data(),
	      _obj(simple_vertex_layout, create_dynamic_buffer<Simple_vertex>(3*2*10)) { // initial size = 10 letters
	}

	void Text_dynamic::set(const std::string& str, bool monospace) {
		_data.clear();
		_font->calculate_vertices(str, _data, monospace);
		_obj.buffer().set(_data);

		auto top_left     = glm::vec2{9999,9999};
		auto bottom_right = glm::vec2{0,0};
		for(auto& v : _data) {
			if(v.xy.x<top_left.x) top_left.x=v.xy.x;
			if(v.xy.y<top_left.y) top_left.y=v.xy.y;

			if(v.xy.x>bottom_right.x) bottom_right.x=v.xy.x;
			if(v.xy.y>bottom_right.y) bottom_right.y=v.xy.y;
		}

		_size = bottom_right - top_left;
	}


	namespace {
		std::unique_ptr<Shader_program> font_shader;

		auto create_text_draw_cmd(glm::vec2 center, glm::vec4 color, float scale,
		                          glm::vec2 size, const Object& obj, const Texture& tex) {
			auto cmd = create_command()
				.shader(*font_shader)
				.texture(Texture_unit::color, tex)
				.object(obj)
				.order_dependent()
				.require_not(Gl_option::depth_write);

			auto trans = glm::scale(glm::translate(glm::mat4(),
			                            glm::vec3(center.x-size.x/2*scale,
			                                      center.y+size.y/2*scale,
			                                      0.f)),
			                            {scale, scale,1});
			cmd.uniforms()
			        .emplace("model", trans)
			        .emplace("color", color);

			return cmd;
		}
	}

	void init_font_renderer(asset::Asset_manager& assets) {
		font_shader = std::make_unique<Shader_program>();
		font_shader->attach_shader(assets.load<Shader>("vert_shader:simple"_aid))
		           .attach_shader(assets.load<Shader>("frag_shader:simple"_aid))
		           .bind_all_attribute_locations(simple_vertex_layout)
		           .build()
		           .uniforms(make_uniform_map(
		                "tex", int(Texture_unit::color),
		                "clip", glm::vec4(0,0,1,1),
		                "layer", 1.0f
		           ));
	}

	void Text::draw(Command_queue& queue, glm::vec2 center,
	                glm::vec4 color, float scale)const {
		queue.push_back(create_text_draw_cmd(center, color, scale, size(), _obj, *_font->_texture));
	}
	void Text_dynamic::draw(Command_queue& queue, glm::vec2 center,
	                        glm::vec4 color, float scale)const {
		queue.push_back(create_text_draw_cmd(center, color, scale, size(), _obj, *_font->_texture));
	}

}
}
