#include "level_settings.hpp"

#include <core/gui/gui.hpp>
#include <core/gui/translations.hpp>
#include <core/utils/command.hpp>


namespace lux {
namespace editor {

	namespace {
		void apply_metadata(Meta_system& systems, const Level_info& data) {
			systems.light_config(data.environment_light_color,
			                     data.environment_light_direction,
			                     data.ambient_brightness,
			                     data.background_tint,
			                     data.environment_brightness );
		}

		class Metadata_update_cmd : public util::Command {
			public:
				Metadata_update_cmd(Level_settings& settings,
				                    Meta_system& systems, Level_info& live, Level_info next)
				    : _settings(settings),
				      _systems(systems),
				      _prev_metadata(next),
				      _live_metadata(live) {
				}

				void execute() override {
					std::swap(_prev_metadata, _live_metadata);
					apply_metadata(_systems, _live_metadata);
					if(!_first_exec) {
						_settings.metadata_modified();
					}
					_first_exec = false;
				}
				void undo() override {
					std::swap(_prev_metadata, _live_metadata);
					_settings.metadata_modified();
					apply_metadata(_systems, _live_metadata);
				}
				auto name()const -> const std::string& override {
					return _name;
				}

			private:
				static const std::string _name;

				Level_settings& _settings;
				bool _first_exec = true;
				Meta_system& _systems;
				Level_info _prev_metadata;
				Level_info& _live_metadata;
		};
		const std::string Metadata_update_cmd::_name = "Metadata changed";
	}

	struct Level_settings::PImpl {
		std::string last_level_id;
		gui::Text_edit input_name;
		gui::Text_edit input_desc;
		gui::Text_edit input_author;
		const util::Command* last_update_cmd = nullptr;
	};

	Level_settings::Level_settings(gui::Gui& gui, const gui::Translator& translator,
	                               Meta_system& systems, util::Command_manager& commands,
	                               Level_info& metadata)
	    : _impl(std::make_unique<PImpl>()),
	      _gui(gui),
	      _translator(translator),
	      _systems(systems),
	      _commands(commands),
	      _metadata(metadata) {
	}
	Level_settings::~Level_settings() = default;

	void Level_settings::metadata_modified() {
		_impl->last_level_id.clear();
	}

	void Level_settings::update_and_draw() {
		if(!_visible)
			return;

		if(_impl->last_level_id!=_metadata.id) {
			_impl->last_level_id = _metadata.id;
			_impl->input_name.reset(_metadata.name);
			_impl->input_desc.reset(_metadata.description);
			_impl->input_author.reset(_metadata.author);
		}

		auto text = [&](auto& key) {
			return _translator.translate("editor", key).c_str();
		};


		auto ctx = _gui.ctx();
		if (nk_begin_titled(ctx, "settings", text("settings"), _gui.centered(400, 800),
		             NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE|
		             NK_WINDOW_TITLE|NK_WINDOW_SCALABLE|NK_WINDOW_SCROLL_AUTO_HIDE)) {

			auto data = _metadata; // temporary copy

			constexpr auto row_h = 25;

			nk_layout_row_dynamic(ctx, row_h, 1);

			nk_label(ctx, text("name"), NK_TEXT_LEFT);
			_impl->input_name.update_and_draw(ctx, NK_EDIT_FIELD, data.name);

			nk_label(ctx, text("author"), NK_TEXT_LEFT);
			_impl->input_author.update_and_draw(ctx, NK_EDIT_FIELD, data.author);

			nk_label(ctx, text("description"), NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, 100, 1);
			_impl->input_desc.update_and_draw(ctx, NK_EDIT_BOX, data.description);
			nk_layout_row_dynamic(ctx, row_h, 1);

			nk_layout_row_dynamic(ctx, row_h*2, 1);
			nk_label(ctx, text("ambient light"), NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, row_h, 1);

			data.ambient_brightness = nk_propertyf(ctx, text("intensity"), 0,
			                                       data.ambient_brightness,
			                                       2.0, 0.01f,0.01f);

			nk_layout_row_dynamic(ctx, row_h*2, 1);
			nk_label(ctx, text("sun"), NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, row_h, 1);

			gui::color_picker(ctx, data.environment_light_color, 400, 10.f);
			nk_layout_row_dynamic(ctx, row_h, 1);

			auto& dir = data.environment_light_direction;
			dir.x = nk_propertyf(ctx, "X:", -1.0, dir.x, 1.0, 0.01f,0.01f);
			dir.y = nk_propertyf(ctx, "Y:", -1.0, dir.y, 1.0, 0.01f,0.01f);
			dir.z = nk_propertyf(ctx, "Z:", -1.0, dir.z, 1.0, 0.01f,0.01f);

			nk_layout_row_dynamic(ctx, row_h*2, 1);

			nk_label(ctx, text("background"), NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, row_h, 1);
			data.environment_brightness = nk_propertyf(ctx, text("brightness"), 0,
			                                           data.environment_brightness,
			                                           1.0, 0.02f,0.005f);
			gui::color_picker(ctx, data.background_tint, 400);
			nk_layout_row_dynamic(ctx, row_h, 1);

			if(data!=_metadata) {
				// reuse last command if nothing else has happend (avoids >9000 commands for gradual modifications)
				if(_commands.is_last(_impl->last_update_cmd)) {
					_metadata = std::move(data);
					apply_metadata(_systems, _metadata);

				} else {
					_impl->last_update_cmd = &_commands.execute<Metadata_update_cmd>(*this,
					                                                                 _systems,
					                                                                 _metadata,
					                                                                 std::move(data) );
				}
			}

		} else {
			_visible = false;
		}
		nk_end(ctx);
	}

}
}
