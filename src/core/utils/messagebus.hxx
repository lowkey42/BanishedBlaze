#pragma once

#ifndef MESSAGEBUS_HPP_INCLUDED
	#include "messagebus.hpp"
#endif

namespace mo {
namespace util {

	template<class T>
	Mailbox<T>::Mailbox(Message_bus& bus, std::size_t size) : _queue(size,0,4), _bus(bus) {
		_bus.register_mailbox(*this);
	}

	template<class T>
	Mailbox<T>::~Mailbox() {
		_bus.unregister_mailbox(*this);
	}

	template<class T>
	void Mailbox<T>::send(const T& v) {
		_queue.enqueue(v);
	}

	template<class T>
	auto Mailbox<T>::receive() -> maybe<T> {
		maybe<T> ret = T{};

		if(!_queue.try_dequeue(ret.get_or_throw()))
			ret = nothing();

		return ret;
	}

	template<class T>
	template<std::size_t Size>
	auto Mailbox<T>::receive(T (&target)[Size]) -> std::size_t {
		return _queue.try_dequeue_bulk(target, Size);
	}

	template<class T>
	auto Mailbox<T>::empty()const noexcept -> bool {
		return _queue.size_approx() <= 0;
	}


	inline Mailbox_collection::Mailbox_collection(Message_bus& bus) : _bus(bus) {
	}

	namespace details {
		template<class T, std::size_t size, typename Func>
		void receive_bulk(void* box, Func& handler) {
			using namespace std;

			INVARIANT(box, "No subscription for given type");

			T msg[size];
			auto& cbox = *static_cast<Mailbox<T>*>(box);

			auto count = std::size_t(0);
			do {
				count = cbox.receive(msg);

				for_each(begin(msg), begin(msg)+count, handler);
			} while(count>0);
		}
	}

	template<class T, std::size_t bulk_size, typename Func>
	void Mailbox_collection::subscribe(std::size_t queue_size, Func handler) {
		using namespace std;

		auto& box = _boxes[typeuid_of<T>()];
		box.box = make_shared<Mailbox<T>>(_bus, queue_size);
		box.handler = [handler](Sub& s) {
			details::receive_bulk<T, bulk_size>(s.box.get(), handler);
		};
	}
	template<std::size_t bulk_size,
	         std::size_t queue_size,
	         typename... Func>
	void Mailbox_collection::subscribe_to(Func&&... handler) {
		apply([&](auto&& h) {
			using T = std::decay_t<nth_func_arg_t<std::remove_reference_t<decltype(h)>, 0>>;
			this->subscribe<T, bulk_size>(queue_size, std::forward<decltype(h)>(h));

		}, std::forward<Func>(handler)...);
	}

	template<class T>
	void Mailbox_collection::unsubscribe() {
		_boxes.erase(typeuid_of<T>());
	}

	template<class T, std::size_t size, typename Func>
	void Mailbox_collection::receive(Func handler) {
		auto& box = _boxes[typeuid_of<T>()];
		details::receive_bulk<size>(box.box.get(), handler);
	}

	inline void Mailbox_collection::update_subscriptions() {
		for(auto& b : _boxes)
			if(b.second.handler)
				b.second.handler(b.second);
	}

	template<typename Msg>
	void Mailbox_collection::send_msg(const Msg& msg, Typeuid self) {
		_bus.send_msg(msg, self);
	}


	template<typename T>
	Message_bus::Mailbox_ref::Mailbox_ref(Mailbox<T>& mailbox, Typeuid self)
	  : _self(self), _type(typeuid_of<T>()),
		_mailbox(static_cast<void*>(&mailbox)),
		_send(+[](void* mb, const void* m){
			static_cast<Mailbox<T>*>(mb)->send(*static_cast<const T*>(m));
		}) {
	}

	template<typename T>
	void Message_bus::Mailbox_ref::exec_send(const T& m, Typeuid self) {
		assert(_type==typeuid_of<T>() && "Types don't match");

		if(_self!=0 && self==_self) {
			return;
		}

		_send(_mailbox, static_cast<const void*>(&m));
	}

	template<typename T>
	void Message_bus::register_mailbox(Mailbox<T>& mailbox, Typeuid self) {
		// TODO: mutex
		_add_queue.emplace_back(mailbox, self);
	}

	template<typename T>
	void Message_bus::unregister_mailbox(Mailbox<T>& mailbox) {
		// TODO: mutex
		_remove_queue.emplace_back(mailbox);
	}

	inline void Message_bus::update() {
		for(auto& m : _add_queue) {
			group(m._type).emplace_back(std::move(m));
		}
		_add_queue.clear();

		for(auto& m : _remove_queue) {
			util::erase_fast(group(m._type), m);
		}
		_remove_queue.clear();

		for(auto& c : _children)
			c->update();
	}


	template<typename Msg>
	void Message_bus::send_msg(const Msg& msg, Typeuid self) {
		auto id = typeuid_of<Msg>();

		if(std::size_t(id) <_mb_groups.size()) {
			for(auto& mb : _mb_groups[id]) {
				mb.exec_send(msg, self);
			}
		}

		for(auto& c : _children)
			c->send_msg(msg, self);
	}

	inline Message_bus::Message_bus() : _parent(nullptr) {
	}
	inline Message_bus::Message_bus(Message_bus* parent) : _parent(parent) {
		_parent->_children.push_back(this);
	}
	inline Message_bus::~Message_bus() {
		if(_parent) {
			util::erase_fast(_parent->_children, this);
		}
	}

	inline auto Message_bus::create_child() -> Message_bus {
		return Message_bus(this);
	}

}
}
