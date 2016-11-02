#pragma once

#ifndef ECS_INCLUDED
#include "ecs.hpp"
#endif

#define unlikely(X) (__builtin_expect(X, false))

namespace lux {
namespace ecs {
	
	
	template<typename T>
	void Entity_manager::register_component_type() {
		auto type = component_type_id<T>();
		
		if(unlikely(static_cast<Component_type>(_components.size())<=type)) {
			_components.resize(type + 1);
		}
		
		auto& container_ptr = _components[type];
		
		if(unlikely(!container_ptr)) {
			container_ptr = std::make_unique<Component_container<T>>(*this);
			
			auto it = _components_by_name.emplace(T::name(), type);
			INVARIANT(it.first->second==type, "Multiple components with same name: "<<T::name());
		}
	}
	inline auto Entity_manager::component_type_by_name(const std::string& name) -> util::maybe<Component_type> {
		auto it = _components_by_name.find(name);
		return it!=_components_by_name.end() ? util::just(it->second) : util::nothing();
	}
	
	template<typename C>
	auto Entity_manager::list() -> Component_container<C>& {
		auto type = component_type_id<C>();
		
		if(unlikely(static_cast<Component_type>(_components.size())<=type || !_components[type])) {
			register_component_type<C>();
		}
		
		return static_cast<Component_container<C>&>(*_components[type]);
	}
	inline auto Entity_manager::list(Component_type type) -> Component_container_base& {
		INVARIANT(static_cast<Component_type>(_components.size())>type && _components[type],
		          "Invalid/unregistered component type: "<<((int) type));
		return *_components[type];
	}

	template<typename F>
	void Entity_manager::list_all(F&& handler) {
		for(auto& container : _components) {
			if(container) {
				handler(*container);
			}
		}
	}
	
	
	template<typename T>
	util::maybe<T&> Entity_facet::get() {
		INVARIANT(_manager && _manager->validate(_owner), "Access to invalid Entity_facet for "<<entity_name(_owner));
		return _manager->list<T>().find(_owner);
	}

	template<typename T>
	bool Entity_facet::has() {
		return _manager && _owner && _manager->list<T>().has(_owner);
	}

	template<typename T, typename... Args>
	void Entity_facet::emplace(Args&&... args) {
		emplace_init<T>(+[](const T&){}, std::forward<Args>(args)...);
	}

	template<typename T, typename F, typename... Args>
	void Entity_facet::emplace_init(F&& init, Args&&... args) {
		INVARIANT(_manager && _manager->validate(_owner), "Access to invalid Entity_facet for "<<entity_name(_owner));
		_manager->list<T>().emplace(std::forward<F>(init), _owner, std::forward<Args>(args)...);
	}

	template<typename T>
	void Entity_facet::erase() {
		INVARIANT(_manager && _manager->validate(_owner), "Access to invalid Entity_facet for "<<entity_name(_owner));
		return _manager->list<T>().erase(_owner);
	}

	namespace detail {
		inline bool ppack_and() {
			return true;
		}

		template<class FirstArg, class... Args>
		bool ppack_and(FirstArg&& first, Args&&... args) {
			return first && ppack_and(std::forward<Args>(args)...);
		}
	}
	template<typename... T>
	void Entity_facet::erase_other() {
		INVARIANT(_manager && _manager->validate(_owner), "Access to invalid Entity_facet for "<<entity_name(_owner));

		for(auto& pool : _manager->_components) {
			if(pool && detail::ppack_and(pool->value_type()!=component_type_id<T>()...)) {
				pool->erase(_owner);
			}
		}
	}

}
}
