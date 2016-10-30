/** Reads & writes entites to disc *******************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include "types.hpp"
#include "../asset/asset_manager.hpp"

#include <sf2/sf2.hpp>

#include <string>
#include <unordered_map>


namespace lux {
namespace asset {
	class AID;
	class Asset_manager;
}

namespace ecs {

	struct Serializer : public sf2::JsonSerializer {
		Serializer(std::ostream& stream, Entity_manager& m,
		           asset::Asset_manager& assets,
		           Component_filter filter={})
			: sf2::JsonSerializer(stream),
			  manager(m), assets(assets), filter(filter) {
		}

		Entity_manager& manager;
		asset::Asset_manager& assets;
		Component_filter filter;
	};
	struct Deserializer : public sf2::JsonDeserializer {
		Deserializer(const std::string& source_name,
		             std::istream& stream, Entity_manager& m,
		             asset::Asset_manager& assets,
		             Component_filter filter={});

		Entity_manager& manager;
		asset::Asset_manager& assets;
		Component_filter filter;
	};

	extern Component_type blueprint_comp_id;
	
	extern void apply_blueprint(asset::Asset_manager&, Entity_facet e,
	                            const std::string& blueprint);
	
	extern void load(sf2::JsonDeserializer& s, Entity_handle& e);
	extern void save(sf2::JsonSerializer& s, const Entity_handle& e);


	// Fallbacks for components that don't require/provide a serialization
	template<class T>
	void load_component(ecs::Deserializer& state, T&) {
		state.read_lambda([](auto&&){
			return true;
		});
	}

	template<class T>
	void save_component(ecs::Serializer& state, const T&) {
		state.write_virtual();
	}

}
}
