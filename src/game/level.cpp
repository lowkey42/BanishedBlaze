#include "level.hpp"

#include "sys/physics/transform_comp.hpp"
#include "sys/renderer/terrain_comp.hpp"

#include <core/ecs/serializer.hpp>
#include <core/asset/asset_manager.hpp>
#include <core/utils/sf2_glm.hpp>

#include <unordered_set>


namespace lux {

	namespace {
		struct Level_list {
			std::vector<std::string> levels;
		};

		sf2_structDef(Level_list,
			levels
		)
	}

	sf2_structDef(Level_info,
		id,
		name,
		author,
		description,
		pack,
		environment_id,
		environment_brightness,
		environment_light_color,
		environment_light_direction,
		ambient_brightness,
		background_tint,
		music_id
	)


	sf2_structDef(Level_pack_entry,
		aid,
		position
	)

	sf2_structDef(Level_pack,
		name,
		description,
		level_ids,
		icon_texture_id,
		map_texture_id
	)


	namespace {
		auto level_aid(const std::string& id) {
			return asset::AID{"level"_strid, id+".map"};
		}

		void add_to_list(Engine& engine, const std::string& id) {
			auto level_list = engine.assets().load_maybe<Level_list>("cfg:my_levels"_aid).process(Level_list{}, [](auto p){return *p;});

			if(std::find(level_list.levels.begin(), level_list.levels.end(),
			             id) == level_list.levels.end()) {

				level_list.levels.push_back(id);
				engine.assets().save<Level_list>("cfg:my_levels"_aid, level_list);
			}
		}

		constexpr auto epsilon = 0.00001f;

		template<class T>
		bool is_vec_same(const T& lhs, const T& rhs) {
			return glm::all(glm::lessThan(glm::abs(lhs-rhs), T(epsilon)));
		}
	}


	bool Level_info::operator==(const Level_info& rhs)const noexcept {
		return id==rhs.id
		        && name==rhs.name
		        && author==rhs.author
		        && description==rhs.description
		        && pack==rhs.pack
		        && environment_id==rhs.environment_id
		        && std::abs(environment_brightness-rhs.environment_brightness)<epsilon
		        && is_vec_same(environment_light_color, rhs.environment_light_color)
		        && is_vec_same(environment_light_direction, rhs.environment_light_direction)
		        && std::abs(ambient_brightness-rhs.ambient_brightness)<epsilon
		        && is_vec_same(background_tint, rhs.background_tint)
		        && music_id==rhs.music_id;
	}

	auto list_local_levels(Engine& engine) -> std::vector<Level_info_ptr> {
		auto level_list = engine.assets().load<Level_list>("cfg:levels"_aid);
		auto my_level_list = engine.assets().load_maybe<Level_list>("cfg:my_levels"_aid).process(Level_list{}, [](auto p){return *p;});

		auto ret = std::vector<Level_info_ptr>();
		ret.reserve(level_list->levels.size() + my_level_list.levels.size());
		for(auto& lid : level_list->levels) {
			ret.emplace_back(engine.assets().load<Level_info>(level_aid(lid)));
		}
		for(auto& lid : my_level_list.levels) {
			ret.emplace_back(engine.assets().load<Level_info>(level_aid(lid)));
		}

		return ret;
	}
	auto get_level(Engine& engine, const std::string& id) -> Level_info_ptr {
		return engine.assets().load<Level_info>(level_aid(id));
	}
	auto load_level(Engine& engine, ecs::Entity_manager& ecs, const std::string& id) -> util::maybe<Level_info> {
		auto data = engine.assets().load_raw(level_aid(id));

		if(data.is_nothing()) {
			return util::nothing();
		}

		auto& stream = data.get_or_throw();

		Level_info level_data;
		sf2::deserialize_json(stream, [&](auto& msg, uint32_t row, uint32_t column) {
			ERROR("Error parsing LevelData from "<<id<<" at "<<row<<":"<<column<<": "<<msg);
		}, level_data);

		ecs.read(stream, true);

		return level_data;
	}

	void save_level(Engine& engine, ecs::Entity_manager& ecs, const Level_info& level) {
		auto stream = engine.assets().save_raw(level_aid(level.id));

		sf2::serialize_json(stream, level);

		stream<<std::endl; // line-break and flush

		ecs.write(stream, [](ecs::Component_type comp) {
			return comp==ecs::component_type_id<sys::physics::Transform_comp>()
			        || comp==ecs::blueprint_comp_id
			        || comp==ecs::component_type_id<sys::renderer::Terrain_data_comp>();
		});

		stream.close();

		add_to_list(engine, level.id);
	}

	auto Level_pack::find_level(std::string id)const -> util::maybe<int> {
		auto r = std::find_if(level_ids.begin(), level_ids.end(), [id](auto& l) {
			return l.aid ==id;
		});

		return r!=level_ids.end() ? util::just(static_cast<int>(std::distance(level_ids.begin(),r)))
		                          : util::nothing();
	}
	auto list_level_packs(Engine& engine) -> std::vector<Level_pack_ptr> {
		auto aids  = engine.assets().list("level_pack"_strid);
		auto packs = std::vector<Level_pack_ptr>{};
		packs.reserve(aids.size());
		for(auto& aid : aids) {
			packs.emplace_back(get_level_pack(engine, aid.name()));
		}

		return packs;
	}
	auto get_level_pack(Engine& engine, const std::string& id) -> Level_pack_ptr {
		return engine.assets().load<Level_pack>(asset::AID{"level_pack"_strid, id});
	}
	auto get_next_level(Engine& engine, const std::string& level_id) -> util::maybe<std::string> {
		auto level = get_level(engine, level_id);
		if(!level || level->pack.empty())
			return util::nothing();

		auto pack = get_level_pack(engine, level->pack);
		if(!pack)
			return util::nothing();

		auto level_idx = pack->find_level(level_id);
		if(level_idx.is_nothing())
			return util::nothing();

		auto next_idx = static_cast<std::size_t>(level_idx.get_or_throw()+1);
		if(next_idx >= pack->level_ids.size())
			return util::nothing();

		return pack->level_ids.at(next_idx).aid;
	}

	namespace {
		struct Savegame {
			std::unordered_set<std::string> unlocked_levels;
		};
		sf2_structDef(Savegame,
			unlocked_levels
		)
		auto load_savegame(Engine& engine) -> const Savegame& {
			static auto savegame = engine.assets().load_maybe<Savegame>(asset::AID{"cfg"_strid, "savegame"});
			if(savegame.is_nothing()) {
				auto new_savegame = Savegame{};
				engine.assets().save(asset::AID{"cfg"_strid, "savegame"}, new_savegame);
				savegame = engine.assets().load_maybe<Savegame>(asset::AID{"cfg"_strid, "savegame"});
			}
			return *savegame.get_or_throw();
		}
	}

	void unlock_level(Engine& engine, const std::string& id) {
		auto savegame_copy = load_savegame(engine);
		savegame_copy.unlocked_levels.emplace(id);

		engine.assets().save(asset::AID{"cfg"_strid, "savegame"}, savegame_copy);
	}
	void unlock_next_levels(Engine& engine, const std::string& id) {
		get_next_level(engine, id).process([&](auto& next) {
			auto savegame_copy = load_savegame(engine);
			savegame_copy.unlocked_levels.emplace(next);

			engine.assets().save(asset::AID{"cfg"_strid, "savegame"}, savegame_copy);
		});
	}
	auto is_level_locked(Engine& engine, const std::string& id) -> bool {
		const auto& unlocked_levels = load_savegame(engine).unlocked_levels;
		return unlocked_levels.find(id)==unlocked_levels.end();
	}


	auto list_remote_levels(Engine&, int32_t count, int32_t offset) -> std::vector<Level_info> {
		// TODO
		return {};
	}
	auto download_level(Engine&, const std::string& id) -> std::future<void> {
		// TODO
		std::promise<void> dummy;
		auto future = dummy.get_future();
		dummy.set_value();
		return future;
	}
	void upload_level(Engine&, const std::string& id) {
		// TODO
	}

}
