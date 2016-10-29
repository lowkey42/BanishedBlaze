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
		
		if(unlikely(_components.size()<=type)) {
			_components.resize(type + 1);
		}
		
		auto& container_ptr = _components[type];
		
		if(unlikely(!container_ptr)) {
			container_ptr = std::make_unique<Component_container<T>>(*this);
			
			_components_by_name.emplace(t::name(),
			                            Component_type_info{T::name(), type,
			                                                container_ptr.get()});
		}
	}
	
	template<typename C>
	auto Entity_manager::list() -> Component_container<C>& {
		auto type = component_type_id<T>();
		
		if(unlikely(_components.size()>=type || !_components[type])) {
			register_component_type<T>();
		}
		
		return *components[type];
	}
	
	
	template<typename T>
	util::maybe<T&> Entity_facet::get() {
		INVARIANT(_manager && _owner, "Access to invalid Entity_facet");
		return _manager->list<T>().find(_owner);
	}

	template<typename T>
	bool Entity_facet::has() {
		return _manager && _owner && _manager->list<T>().has(_owner);
	}

	template<typename T, typename... Args>
	auto Entity_facet::emplace(Args&&... args) -> T& {
		INVARIANT(_manager && _owner, "Access to invalid Entity_facet");
		return _manager->list<T>().emplace(_owner, std::forward<Args>(args)...);
	}

	template<typename T>
	void Entity_facet::erase() {
		INVARIANT(_manager && _owner, "Access to invalid Entity_facet");
		return _manager->list<T>().erase(_owner);
	}

	namespace {
		bool ppack_and() {
			return true;
		}

		template<class FirstArg, class... Args>
		bool ppack_and(FirstArg&& first, Args&&... args) {
			return first && ppack_and(std::forward<Args>(args)...);
		}
	}
	template<typename... T>
	void Entity_facet::erase_other() {
		for(auto& pool : _manager->_components) {
			if(pool && ppack_and(pool->value_type()!=component_type<T>()...)) {
				pool->erase(_owner);
			}
		}
	}
	
/*
	template<typename Comp>
	auto Entity_manager::list() -> typename Comp::Pool& {
		auto it = _pools[Comp::type()].get();

		if(!it) {
			register_component_type<Comp>();
			it = _pools[Comp::type()].get();
		}

		return *static_cast<typename Comp::Pool*>(it);
	}

	// crazy template-magic to determine components that provide load/store functionality
	namespace details {
		inline Component_base*& get_component(Entity& e, Component_type t) {
			return e._components[t];
		}
		inline Entity_ptr get_entity(Entity& e) {
			return e.shared_from_this();
		}
	}

	template<typename T>
	void Entity_manager::register_component_type() {
		INVARIANT(T::type()<details::max_comp_type, "Set MAX_COMP_TYPE to at least "<<T::type());

		if(_pools[T::type()])
			return;

		using Pool = typename T::Pool;

		Pool* pool = new Pool;
		_pools[T::type()].reset(pool);

		_types.emplace(T::name(), details::Component_type_info{
						  T::name(),
						  T::type(),
						  pool,
						  [pool](Entity& e){pool->create(e);},
						  [pool](Entity& e){return details::get_component(e, T::type());}
		});

		static auto first_call = true;
		if(first_call) {
			DEBUG("Registered component type '"<<T::name()<<"' with id "<<T::type());
			first_call = false;
		}
	}

	// entity

	template<typename T>
	util::maybe<T&> Entity::get() {
		INVARIANT(T::type()<details::max_comp_type, "Access to unregistered component "<<T::name());
		return util::justPtr(static_cast<T*>(details::get_component(*this, T::type())));
	}

	template<typename T>
	util::maybe<T&> Entity::getByType(Component_type type) {
		auto comp = details::get_component(*this, type);
		if(!comp)
			return util::nothing();

		return util::justPtr(dynamic_cast<T*>(comp));
	}

	template<typename T>
	bool Entity::has() {
		INVARIANT(T::type()<details::max_comp_type, "Access to unregistered component "<<T::name());
		return details::get_component(*this, T::type())!=nullptr;
	}

	template<typename T, typename... ARGS>
	T& Entity::emplace(ARGS&&... args) {
		INVARIANT(T::type()<details::max_comp_type, "Access to unregistered component "<<T::name());
		INVARIANT(!has<T>(), "Component already exists: "<<T::name());

		return _manager.list<T>().create(*this, std::forward<ARGS>(args)...);
	}

	template<typename T>
	void Entity::erase() {
		INVARIANT(T::type()<details::max_comp_type, "Access to unregistered component "<<T::name());

		if(has<T>())
			_manager.list<T>().free(*this);
	}
	template<typename... T>
	void Entity::erase_other() {
		bool erase_c[details::max_comp_type];

		for(Component_type c=0; c<details::max_comp_type; ++c)
			erase_c[c] = true;

		for(auto c : {T::type()...})
			erase_c[c] = false;

		for(Component_type c=0; c<details::max_comp_type; ++c) {
			if(erase_c[c] && _components[c])
				_manager._pools[c]->free(*this);
		}
	}

	template<typename T>
	auto Entity::get_handle() -> util::lazy<util::maybe<T&>> {
		INVARIANT(T::type()<details::max_comp_type, "Access to unregistered component "<<T::name());

		auto ref = Entity_weak_ptr(details::get_entity(*this));

		return util::later([ref](){
			auto e = ref.lock();
			return e ? e->get<T>() : util::nothing();
		});
	}
*/
}
}
