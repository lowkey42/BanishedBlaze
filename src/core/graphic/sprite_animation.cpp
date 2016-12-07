#define GLM_SWIZZLE
#define BUILD_SERIALIZER

#include "sprite_animation.hpp"

#include "../utils/messagebus.hpp"
#include "../utils/sf2_glm.hpp"


namespace lux {
namespace graphic {

	using namespace glm;
	using namespace unit_literals;

	namespace {
		enum class Loop_mode {
			no,
			yes,
			next
		};

		struct Animation_event_desc {
			int_fast16_t frame;
			util::Str_id name;
		};

		sf2_enumDef(Loop_mode, no, yes, next)
		sf2_structDef(Animation_event_desc, frame, name)
	}

	struct Sprite_animation_Clip {
		std::vector<glm::vec2> frames;
		float fps;
		Loop_mode loop = Loop_mode::no;
		float width;
		float height;
		glm::vec2 pivot {0.5f, 0.5f};
		int loop_begin = 0;
		std::vector<Animation_event_desc> events;
		Animation_clip_id next = ""_strid;

		auto uv_rect(int_fast16_t frame)const {
			auto offset = frames.at(frame);
			return glm::vec4{offset.x, offset.y, offset.x+width, offset.y+height};
		}
	};

	struct Sprite_animation_set::PImpl {
		Material_ptr _material;
		std::unordered_map<Animation_clip_id, Sprite_animation_Clip> _clips;
		mutable std::vector<Sprite_animation_state*> _instances;
	};

	sf2_structDef(Sprite_animation_Clip, frames, fps, loop, width, height, pivot, loop_begin, events, next)

	Sprite_animation_set::Sprite_animation_set(asset::istream& in) : _impl(std::make_unique<PImpl>()) {
		std::string material_aid;
		auto scale = 1.f;

		auto on_error = [&](auto& msg, uint32_t row, uint32_t column) {
			ERROR("Error parsing JSON from "<<in.aid().str()<<" at "<<row<<":"<<column<<": "<<msg);
		};
		auto reader = sf2::JsonDeserializer{sf2::format::Json_reader{in, on_error}};
		reader.read_virtual(
			sf2::vmember("material", material_aid),
			sf2::vmember("material_scale", scale),
			sf2::vmember("clips", _impl->_clips)
		);

		// post-proccess
		_impl->_material = in.manager().load<Material>(asset::AID{material_aid});
		INVARIANT(_impl->_material, "Couldn't load material for Sprite_animation_set \""
		          <<in.aid().name()<<"\"");

		auto tex_size = glm::vec2{_impl->_material->albedo().width(),
		                          _impl->_material->albedo().height()} * scale;

		for(auto& clip : _impl->_clips) {
			clip.second.width /= tex_size.x;
			clip.second.height /= tex_size.y;
			for(auto& frame : clip.second.frames) {
				frame /= tex_size;
			}
		}
	}
	auto Sprite_animation_set::operator=(Sprite_animation_set&& rhs)noexcept -> Sprite_animation_set& {
		INVARIANT(rhs._impl->_instances.empty(), "Moved from living object, YOU MONSTER!");
		INVARIANT(this!=&rhs, "Moved from self");

		auto instances = std::move(_impl->_instances);
		_impl = std::move(rhs._impl);
		_impl->_instances = std::move(instances);

		for(auto inst : _impl->_instances) {
			inst->reset();
		}

		return *this;
	}
	Sprite_animation_set::~Sprite_animation_set() {
		if(_impl) {
			INVARIANT(_impl->_instances.empty(), "Not unregistered instances in sprite_animation_set.");
		}
	}
	void Sprite_animation_set::_register_inst(Sprite_animation_state& s)const {
		_impl->_instances.emplace_back(&s);
	}

	void Sprite_animation_set::_unregister_inst(Sprite_animation_state& s)const {
		util::erase_fast(_impl->_instances, &s);
	}


	Sprite_animation_state::Sprite_animation_state(void* owner, Sprite_animation_set_ptr animations,
	                                               Animation_clip_id initial)
	    : _owner(owner), _animation_set(std::move(animations)) {

		if(_animation_set) {
			_animation_set->_register_inst(*this);

			if(initial!=""_strid) {
				set_clip(initial);
			}
		}
	}
	Sprite_animation_state::Sprite_animation_state(Sprite_animation_state&& rhs)noexcept
	    : _owner(rhs._owner),
	      _animation_set(std::move(rhs._animation_set)),
	      _curr_clip_id(std::move(rhs._curr_clip_id)),
	      _curr_clip(std::move(rhs._curr_clip)),
	      _frame(rhs._frame),
	      _runtime(rhs._runtime),
	      _speed_factor(rhs._speed_factor) {

		if(_animation_set) {
			_animation_set->_unregister_inst(rhs);
			_animation_set->_register_inst(*this);
		}
	}
	auto Sprite_animation_state::operator=(Sprite_animation_state&& rhs)noexcept -> Sprite_animation_state& {
		INVARIANT(this!=&rhs, "Moved from self");

		_owner = rhs._owner;
		_animation_set = std::move(rhs._animation_set);
		_curr_clip_id = std::move(rhs._curr_clip_id);
		_curr_clip = std::move(rhs._curr_clip);
		_frame = std::move(rhs._frame);
		_runtime = std::move(rhs._runtime);
		_speed_factor = std::move(rhs._speed_factor);

		if(_animation_set) {
			_animation_set->_unregister_inst(rhs);
			_animation_set->_register_inst(*this);
		}

		return *this;
	}
	Sprite_animation_state::~Sprite_animation_state() {
		if(_animation_set) {
			_animation_set->_unregister_inst(*this);
		}
	}

	void Sprite_animation_state::animation_set(Sprite_animation_set_ptr set, Animation_clip_id clip) {
		if(_animation_set) {
			_animation_set->_unregister_inst(*this);
		}
		_animation_set = set;
		_curr_clip_id = clip;

		if(_animation_set) {
			_animation_set->_register_inst(*this);
		}

		reset();
	}

	namespace {
		template <typename T> T sign(T val) {
			return (T(0) < val) - (val < T(0));
		}
	}
	void Sprite_animation_state::update(Time dt, util::Message_bus& bus) {
		if(!_playing) {
			return;
		}

		_curr_clip.process([&](Sprite_animation_Clip& clip) {
			if(_frame==0 && _runtime<=0_s && clip.events.size()>0 && dt>0_s && _speed_factor>0) {
				for(const auto& event : clip.events) {
					if(event.frame==0)
						bus.send<Animation_event>(event.name, _owner);
				}
			}

			_runtime += dt * _speed_factor;

			auto frame_skip = static_cast<int_fast16_t>(std::floor(_runtime/1_s * clip.fps));
			_runtime -= (frame_skip / clip.fps) * 1_s;
			auto next_frame = _frame + frame_skip;
			if(next_frame>=static_cast<int>(clip.frames.size())) {
				if(_queued_clip_id.is_some()) {
					set_clip(_queued_clip_id.get_or_throw());
				}

				switch(clip.loop) {
					case Loop_mode::no:
						next_frame = clip.frames.size()-1;
						frame_skip = next_frame - _frame;
						_playing = false;
						break;
					case Loop_mode::yes:
						next_frame = next_frame % clip.frames.size() + clip.loop_begin;
						if(next_frame >= static_cast<int>(clip.frames.size()))
							next_frame = next_frame % clip.frames.size() + clip.loop_begin;

						break;
					case Loop_mode::next:
						set_clip(clip.next);
						return;
				}
			}

			ON_EXIT {
				_frame = next_frame;
				INVARIANT(_frame>=0 && _frame<static_cast<int>(clip.frames.size()), "Frame is out of range: "<<_frame);
			};

			if(frame_skip>0 && clip.events.size()>0) {
				auto step = next_frame - _frame;
				auto step_sign = sign(next_frame - _frame);

				for(const auto& event : clip.events) {
					auto event_step = event.frame - _frame;

					if(event_step<=step && sign(event_step)==step_sign) {
						bus.send<Animation_event>(event.name, _owner);
					}
				}
			}
		});
	}

	auto Sprite_animation_state::uv_rect()const -> vec4 {
		return _curr_clip.process(vec4{0,0,1,1}, [&](auto& a){return a.uv_rect(_frame);});
	}
	auto Sprite_animation_state::material()const -> const Material& {
		return *_animation_set->_impl->_material;
	}

	void Sprite_animation_state::set_clip(Animation_clip_id id) {
		if(_curr_clip_id!=id) {
			auto& clips = _animation_set->_impl->_clips;
			auto clip_iter = clips.find(id);

			if(clip_iter==clips.end() && _curr_clip_id==""_strid) {
				clip_iter = clips.find("idle"_strid);
				if(clip_iter==clips.end()) {
					clip_iter = clips.begin();
				}
			}

			if(clip_iter!=clips.end()) {
				_curr_clip_id = id;

				_curr_clip = util::justPtr(&clip_iter->second);
				_frame = 0;
				_runtime = 0_s;
				_playing = true;
				_queued_clip_id = util::nothing();
				_speed_factor = 1.f;
			}
		}
	}
	void Sprite_animation_state::reset() {
		auto id = _curr_clip_id;
		_curr_clip_id = "_"_strid;
		set_clip(id);
	}

}
}
