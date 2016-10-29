#pragma once

#include <moodycamel/concurrentqueue.hpp>

#ifndef ECS_COMPONENT_INCLUDED
#include "component.hpp"
#endif

namespace lux {
namespace ecs {


	class Sparse_index_policy {
		public:
			void attach(Entity_id, Component_index);
			void detach(Entity_id);
			void shrink_to_fit();
			auto find(Entity_id) -> util::maybe<Component_index>;
			void clear();

		private:
			std::unordered_map<Entity_id, Component_index> _table;
	};
	class Compact_index_policy {
		public:
			void attach(Entity_id, Component_index);
			void detach(Entity_id);
			void shrink_to_fit();
			auto find(Entity_id) -> util::maybe<Component_index>;
			void clear();

		private:
			std::vector<Component_index> _table;
	};


	template<class T>
	struct Pool_storage_policy_value_traits {
		static constexpr bool supports_empty_values = true;
		static constexpr int_fast32_t max_free = 8;
		using Marker_type = Entity_handle;
		static constexpr Marker_type free_mark = invalid_entity;

		static constexpr const Marker_type* marker_addr(const T* inst) {
			return T::component_base_t::marker_addr(inst);
		}
	};

	template<std::size_t Chunk_size, class T>
	class Pool_storage_policy {
			using pool_t = util::pool<T, Chunk_size, Component_index, Pool_storage_policy_value_traits<T>>;
		public:
			using iterator = typename pool_t::iterator;

			template<class... Args>
			auto emplace(Args&&... args) -> std::tuple<T&, Component_index> {
				return _pool.emplace_back(std::forward<Args>(args)...);
			}

			void replace(Component_index idx, T&& new_element) {
				_pool.replace(idx, new_element);
			}

			void erase(Component_index idx) {
				_pool.erase(idx);
			}

			void clear() {
				_pool.clear();
			}

			void shrink_to_fit() {
				_pool.shrink_to_fit();
			}

			auto get(Component_index idx) -> T& {
				return _pool.get(idx);
			}

			auto begin()noexcept -> iterator {
				return _pool.begin();
			}
			auto end()noexcept -> iterator {
				return _pool.end();
			}
			auto size()const -> Component_index {
				return _pool.size();
			}
			auto empty()const -> bool {
				return _pool.empty();
			}

		private:
			pool_t _pool;
	};



	template<class T>
	class Component_container : public Component_container_base {
		friend class Entity_manager;
		public:
			Component_container(Entity_manager& m) : _manager(m) {}


		protected:
			auto value_type()const noexcept -> Component_type override {
				return component_type_id<T>();
			}
			
			void* emplace_or_find_now(Entity_handle owner) override {
				auto entity_id = get_entity_id(owner, _manager);
				if(entity_id==invalid_entity_id) {
					FAIL("emplace_or_find_now of component from invalid/deleted entity");
				}

				auto comp_idx =_index.find(entity_id);
				if(comp_idx.is_some()) {
					return _storage.get(comp_idx.get_or_throw());
				}

				auto comp = _storage.emplace(_manager, owner);
				_index.attach(entity_id, std::get<1>(comp));

				return static_cast<void*>(&std::get<0>(comp));
			}

			util::maybe<void*> find_now(Entity_handle owner) override {
				return find(owner).process([](auto& comp) {
					return static_cast<void*>(&comp);
				});
			}

			template<typename... Args>
			void emplace(Entity_handle owner, Args&&... args) {
				_queued_insertions.enqueue(owner, T(_manager, owner, std::forward<Args>(args)...));
			}

			void erase(Entity_handle owner)override {
				_queued_deletions.enqueue(owner);
			}

			auto find(Entity_handle owner) -> util::maybe<T&> {
				auto entity_id = get_entity_id(owner, _manager);

				return _index.find(entity_id).process([&](auto comp_idx) {
					return _storage.get(comp_idx);
				});
			}
			auto has(Entity_handle owner)const -> bool {
				auto entity_id = get_entity_id(owner, _manager);

				return _index.find(entity_id).is_some();
			}

			void clear() override {
				_queued_clear = true;
			}

			void process_queued_actions() override {
				process_deletions();
				process_insertions();

				bool cleared = false;
				if(_queued_clear.exchange(false)) {
					_queued_deletions = Queue<Entity_handle>{}; // clear by moving a new queue into the old
					_queued_insertions = Queue<Insertion>{}; // clear by moving a new queue into the old
					_index.clear();
					_storage.clear();
					cleared = true;
				}

				if(cleared || _unoptimized_deletes>32) { // TODO
					_unoptimized_deletes = 0;
					_index.shrink_to_fit();
					_storage.shrink_to_fit();
				}
			}

			void process_deletions() {
				std::array<Entity_handle, 16> deletions_buffer;

				do {
					std::size_t deletions = _queued_deletions.try_dequeue_bulk(deletions_buffer, deletions_buffer.size());
					if(deletions>0) {
						for(auto i=0ull; i<deletions; i++) {
							auto entity_id = get_entity_id(deletions_buffer[i], _manager);
							if(entity_id==invalid_entity_id) {
								WARN("Discard delete of component from invalid/deleted entity");
								continue;
							}

							auto comp_idx = _index.get(entity_id);
							_index.detach(entity_id);


							Insertion insertion;
							if(_queued_insertions.try_dequeue(insertion)) {
								auto entity_id = get_entity_id(std::get<1>(insertion), _manager);
								if(entity_id==invalid_entity) {
									WARN("Discard insertion of component from invalid/deleted entity");
									_storage.erase(comp_idx);

								} else {
									_storage.replace(comp_idx, std::move(std::get<0>(insertion)));
									_index.attach(std::get<1>(insertion), comp_idx);
								}
							} else {
								_storage.erase(comp_idx);
								_unoptimized_deletes++;
							}
						}
					} else {
						break;
					}
				} while(true);
			}
			void process_insertions() {
				std::array<Insertion, 8> insertions_buffer;

				do {
					std::size_t insertions = _queued_insertions.try_dequeue_bulk(insertions_buffer, insertions_buffer.size());
					if(insertions>0) {
						for(auto i=0ull; i<insertions; i++) {
							auto entity_id = get_entity_id(std::get<1>(insertions_buffer[i]), _manager);
							if(entity_id==invalid_entity) {
								WARN("Discard insertion of component from invalid/deleted entity");
								continue;
							}

							auto comp = _storage.emplace(std::move(std::get<0>(insertions_buffer[i])));
							_index.attach(entity_id, std::get<1>(comp));
						}
					} else {
						break;
					}
				} while(true);
			}

		public:	
			auto begin()noexcept {
				return _storage.begin();
			}
			auto end()noexcept {
				return _storage.end();
			}
			auto size()const noexcept {
				return _storage.size();
			}
			auto empty()const noexcept {
				return _storage.empty();
			}
			
			using iterator = typename T::storage_policy::iterator;

		private:
			using Insertion = std::tuple<T,Entity_handle>;

			template<class E>
			using Queue = moodycamel::ConcurrentQueue<E>;

			typename T::index_policy   _index;
			typename T::storage_policy _storage;

			Entity_manager&      _manager;
			Queue<Entity_handle> _queued_deletions;
			Queue<Insertion>     _queued_insertions;
			std::atomic<bool>    _queued_clear        {false};
			int                  _unoptimized_deletes = 0;
	};

}
}
