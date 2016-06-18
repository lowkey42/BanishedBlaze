/** simple wrapper for optional values or references *************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <stdexcept>
#include <functional>
#include <memory>
#include "log.hpp"

namespace lux {
namespace util {

	namespace details {
		struct maybe_else_callable {
			bool is_nothing;

			template<typename Func>
			void on_nothing(Func f) {
				if(is_nothing)
					f();
			}
		};
	}

	template<typename T>
	class maybe {
		public:
			/*implicit*/ maybe(T&& data)noexcept : _valid(true), _data(std::move(data)) {}
			/*implicit*/ maybe(const T& data)noexcept : _valid(true), _data(data) {}
			maybe(const maybe& o)noexcept : _valid(o._valid), _data(o._data) {}
			maybe(maybe&& o)noexcept : _valid(o._valid), _data(std::move(o._data)) {
				o._valid = false;
			}

			~maybe()noexcept {
				if(is_some())
					_data.~T();
			}

			operator maybe<const T>()const noexcept {
				return is_some() ? maybe<const T>(_data) : maybe<const T>::nothing();
			}
			operator bool()const noexcept {
				return is_some();
			}

			maybe& operator=(const maybe& o)noexcept {
				_valid = o._valid;
				_data = o._data;
				return *this;
			}
			maybe& operator=(maybe&& o)noexcept {
				if(o._valid) {
					if(_valid)
						_data = std::move(o._data);
					else
						new(&_data) T(std::move(o._data));

					o._data.~T();
				}

				_valid = o._valid;
				o._valid = false;
				return *this;
			}

			static maybe nothing() noexcept {
				return maybe();
			}


			bool is_some()const noexcept {
				return _valid;
			}
			bool is_nothing()const noexcept {
				return !is_some();
			}

			T& get_or_throw() {
				INVARIANT(is_some(), "Called getOrThrow on nothing.");

				return _data;
			}
			const T& get_or_throw()const {
				INVARIANT(is_some(), "Called getOrThrow on nothing.");

				return _data;
			}
			T& get_ref_or_other(T& other)noexcept {
				return is_some() ? _data : other;
			}
			const T& get_ref_or_other(const T& other)const noexcept {
				return is_some() ? _data : other;
			}
			T get_or_other(T other)const noexcept {
				return is_some() ? _data : other;
			}

			template<typename Func>
			auto process(Func f)const {
				if(is_some())
					f(_data);

				return details::maybe_else_callable{is_nothing()};
			}

			template<typename Func>
			auto process(Func f) {
				if(is_some())
					f(_data);

				return details::maybe_else_callable{is_nothing()};
			}

			template<typename RT, typename Func>
			auto process(RT def, Func f) -> RT {
				if(is_some())
					return f(_data);

				return def;
			}

			template<typename RT, typename Func>
			auto process(RT def, Func f)const -> RT {
				if(is_some())
					return f(_data);

				return def;
			}

		private:
			maybe() : _valid(false) {}

			bool _valid;
			union {
				T _data;
			};
	};

	struct nothing {
		template<typename T>
		operator maybe<T>()const noexcept {
			return maybe<T>::nothing();
		}
	};

	template<typename T>
	maybe<T> just(T&& inst) {
		return maybe<T>(std::move(inst));
	}
	template<typename T>
	maybe<T> justCopy(const T& inst) {
		return maybe<T>(inst);
	}
	template<typename T>
	maybe<T&> justPtr(T* inst) {
		return inst!=nullptr ? maybe<T&>(*inst) : nothing();
	}

	template<typename T, typename Func>
	auto operator>>(const maybe<T>& t, Func f)
			->  std::enable_if_t<!std::is_same<void,decltype(f(t.get_or_throw()))>::value,
			                     maybe<decltype(f(t.get_or_throw()))>> {
		return t.is_some() ? just(f(t.get_or_throw())) : nothing();
	}

	template<typename T, typename Func>
	auto operator>>(const maybe<T>& t, Func f)
			->  std::enable_if_t<std::is_same<void,decltype(f(t.get_or_throw()))>::value,
			                     void> {
		if(t.is_some())
			f(t.get_or_throw());
	}

	template<typename T>
	bool operator! (const maybe<T>& m) {
		return !m.is_some();
	}


	template<typename T>
	class maybe<T&> {
		public:
			maybe() : _ref(nullptr) {}
			/*implicit*/ maybe(T& data)noexcept : _ref(&data) {}
			template<typename U, class = std::enable_if_t<std::is_convertible<U*, T*>::value> >
			maybe(U& o)noexcept : _ref(&o) {}
			maybe(const maybe& o)noexcept : _ref(o._ref) {}
			maybe(maybe&& o)noexcept : _ref(o._ref) {
				o._ref = nullptr;
			}
			template<typename U, class = std::enable_if_t<std::is_convertible<U*, T*>::value> >
			maybe(const maybe<U>& o)noexcept : _ref(o._ref) {}
			~maybe()noexcept = default;

			operator maybe<const T&>()const noexcept {
				return is_some() ? maybe<const T&>(*_ref) : maybe<const T&>::nothing();
			}
			operator bool()const noexcept {
				return is_some();
			}

			maybe& operator=(const maybe& o)noexcept {
				_ref = o._ref;
				return *this;
			}
			maybe& operator=(maybe&& o)noexcept {
				std::swap(_ref=nullptr, o._ref);
				return *this;
			}

			static maybe nothing() noexcept {
				return maybe();
			}

			bool is_some()const noexcept {
				return _ref!=nullptr;
			}
			bool is_nothing()const noexcept {
				return !is_some();
			}

			T& get_or_throw()const {
				INVARIANT(is_some(), "Called getOrThrow on nothing.");

				return *_ref;
			}
			T& get_or_other(std::remove_const_t<T>& other)const noexcept {
				return is_some() ? *_ref : other;
			}
			const T& get_or_other(const T& other)const noexcept {
				return is_some() ? *_ref : other;
			}

			template<typename Func>
			auto process(Func f)const {
				if(is_some())
					f(get_or_throw());

				return details::maybe_else_callable{is_nothing()};
			}

			template<typename RT, typename Func>
			auto process(RT def, Func f) -> RT {
				if(is_some())
					return f(get_or_throw());

				return def;
			}

			template<typename RT, typename Func>
			auto process(RT def, Func f)const -> RT {
				if(is_some())
					return f(get_or_throw());

				return def;
			}

		private:
			T* _ref;
	};

	namespace details {
		template<int ...>
		struct seq { };

		template<int N, int ...S>
		struct gens : gens<N-1, N-1, S...> { };

		template<int ...S>
		struct gens<0, S...> {
		  typedef seq<S...> type;
		};

		template<typename... T>
		struct processor {
			std::tuple<T&&...> args;

			template<typename Func>
			void operator>>(Func&& f) {
				call(std::forward<Func>(f), typename gens<sizeof...(T)>::type());
			}

			private:
				template<typename Func, int ...S>
				void call(Func&& f, seq<S...>) {
					call(std::forward<Func>(f), std::forward<decltype(std::get<S>(args))>(std::get<S>(args))...);
				}

				template<typename Func, typename... Args>
				void call(Func&& f, Args&&... m) {
					for(bool b : {m.is_some()...})
						if(!b)
							return;

					f(m.get_or_throw()...);
				}
		};
	}

	/*
	 * Usage:
	 * maybe<bool> b = true;
	 *	maybe<int> i = nothing();
	 *	maybe<float> f = 1.0f;
	 *
	 *	process(b,i,f)>> [](bool b, int i, float& f){
	 *		// ...
	 *	};
	 */
	template<typename... T>
	auto process(T&&... m) -> details::processor<T...> {
		return details::processor<T...>{std::tuple<decltype(m)...>(std::forward<T>(m)...)};
	}

	template<class Map, class Key>
	auto find_maybe(Map& map, const Key& key) -> auto {
		auto iter = map.find(key);
		return iter!=map.end() ? justPtr(&iter->second) : nothing();
	}

	template<typename T>
	class lazy {
		public:
			using source_t = std::function<T()>;

			/*implicit*/ lazy(source_t s) : _source(s){}

			operator T(){
				return _source;
			}

		private:
			source_t _source;
	};

	template<typename T>
	inline lazy<T> later(typename lazy<T>::source_t f) {
		return lazy<T>(f);
	}


	template <class F>
	struct return_type;

	template <class R, class T, class... A>
	struct return_type<R (T::*)(A...)>
	{
	  typedef R type;
	};
	template <class R, class... A>
	struct return_type<R (*)(A...)>
	{
	  typedef R type;
	};

	template<typename S, typename T>
	inline lazy<T> later(S* s, T (S::*f)()) {
		std::weak_ptr<S> weak_s = s->shared_from_this();

		return lazy<T>([weak_s, f](){
			auto shared_s = weak_s.lock();
			if(shared_s) {
				auto s = shared_s.get();
				return (s->*f)();

			} else {
				return T{};
			}
		});
	}

}
}

#ifdef LUX_DEFINE_MAYBE_MACROS
	#define LUX_NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N
	#define LUX_NARGS(...) LUX_NARGS_SEQ(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

	/* This will let macros expand before concating them */
	#define LUX_PRIMITIVE_CAT(x, y) x ## y
	#define LUX_CAT(x, y) LUX_PRIMITIVE_CAT(x, y)

	#define LUX_APPLY(macro, ...) LUX_CAT(LUX_APPLY_, LUX_NARGS(__VA_ARGS__))(macro, __VA_ARGS__)
	#define LUX_APPLY_1(m, x1) m(x1)
	#define LUX_APPLY_2(m, x, ...) m(x), LUX_APPLY_1(m,__VA_ARGS__)
	#define LUX_APPLY_3(m, x, ...) m(x), LUX_APPLY_2(m,__VA_ARGS__)
	#define LUX_APPLY_4(m, x, ...) m(x), LUX_APPLY_3(m,__VA_ARGS__)
	#define LUX_APPLY_5(m, x, ...) m(x), LUX_APPLY_4(m,__VA_ARGS__)
	#define LUX_APPLY_6(m, x, ...) m(x), LUX_APPLY_5(m,__VA_ARGS__)
	#define LUX_APPLY_7(m, x, ...) m(x), LUX_APPLY_6(m,__VA_ARGS__)
	#define LUX_APPLY_8(m, x, ...) m(x), LUX_APPLY_7(m,__VA_ARGS__)
	#define LUX_APPLY_9(m, x, ...) m(x), LUX_APPLY_8(m,__VA_ARGS__)
	#define LUX_APPLY_10(m, x, ...) m(x), LUX_APPLY_9(m,__VA_ARGS__)

	#define LUX_PROCESS_MAYBE_ARG_EXP(name) decltype(name.get_or_throw()) name

	#define LUX_PROCESS_MAYBE(...) ::lux::util::process(__VA_ARGS__) >> [&](LUX_APPLY(LUX_PROCESS_MAYBE_ARG_EXP, __VA_ARGS__))
#endif
