#include "material.hpp"

#include "command_queue.hpp"


namespace lux {
namespace graphic {

	namespace {
		struct Material_desc {
			std::string albedo, normal, material, height;
			bool alpha = false;
		};

		sf2_structDef(Material_desc, albedo, normal, material, height, alpha)

		Texture_ptr black;
		Texture_ptr white;
		Texture_ptr material;
		Texture_ptr normal;
	}

	Material::Material(asset::istream in) {
		Material_desc desc;

		sf2::deserialize_json(in, [&](auto& msg, uint32_t row, uint32_t column) {
			ERROR("Error parsing material from "<<in.aid().str()<<" at "<<row<<":"<<column<<": "<<msg);
		}, desc);

		auto load_or_default = [&](const auto& aid, auto& def) {
			return aid.empty() ? def : in.manager().load<Texture>(asset::AID(aid));
		};

		_albedo    = load_or_default(desc.albedo, black);
		_normal    = load_or_default(desc.normal, normal);
		_material  = load_or_default(desc.material, material);
		_height    = load_or_default(desc.height, white);
		_alpha     = desc.alpha;
	}

	void Material::set_textures(Command& cmd)const {
		cmd.texture(Texture_unit::color,     *_albedo);
		cmd.texture(Texture_unit::normal,    *_normal);
		cmd.texture(Texture_unit::material,  *_material);
		cmd.texture(Texture_unit::height,    *_height);
		if(_alpha)
			cmd.order_dependent();
	}

	void init_materials(asset::Asset_manager& assets) {
		black = assets.load<Texture>("tex:black"_aid);
		white = assets.load<Texture>("tex:white"_aid);
		normal = assets.load<Texture>("tex:normal"_aid);
		material = assets.load<Texture>("tex:material"_aid);
	}
}
}
