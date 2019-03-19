#pragma once

#include <iostream>
#include <utility>

namespace wws{
	template <bool B, typename T>
	struct has_type;

	template <typename T>
	struct has_type<true, T> {
		using type = T;
		static constexpr bool val = true;
	};

	template <>
	struct has_type<false, void> {
		using type = void;
		static constexpr bool val = false;
	};

	template <bool B, typename T1, typename T2>
	struct choose_if {
		using type = T2;
	};

	template <typename T1, typename T2>
	struct choose_if<true, T1, T2>
	{
		using type = T1;
	};


	template<typename T>
	struct can_cout
	{
		template<typename U>
		static auto func(int) -> decltype((std::declval<std::ostream&>() << std::declval<U>()), std::true_type())
		{
			return std::true_type();
		}
		template<typename U>
		static auto func(...)->std::false_type
		{
			return std::false_type();
		}
		static constexpr bool val = decltype(func<T>(0))::value;
	};

	template <typename T>
	struct Tv {
		using type = T;
		static constexpr bool val = true;
		T t;
		Tv(T t_) : t(t_)
		{

		}
	};

	template <>
	struct Tv<void> {
		using type = void;
		static constexpr bool val = false;
		Tv(int i)
		{

		}
	};

	template<typename T>
	auto dbg_func(const char *expr, Tv<T>&& tv) -> T
	{
		if constexpr (Tv<T>::val)
		{
			if constexpr (can_cout<T>::val)
			{
				std::cout << expr << " = " << tv.t << std::endl;
			}
			else {
				std::cout << expr << " = " << "This type can not print!!!" << std::endl;
			}
			return std::forward<T>(tv.t);
		}
		else {
			std::cout << expr << " = void" << std::endl;
		}
	}

	template<typename ...T>
	struct FirstArge;

	template<typename T>
	struct FirstArge<T>
	{
		using type = T;
	};

	template<>
	struct FirstArge<>
	{
		using type = Tv<void>;
	};

	template<bool B, typename... V>
	auto dbg_choose(const char *p, V&& ... t) -> typename FirstArge<V...>::type::type
	{
		if constexpr (B)
		{
			return dbg_func(p, std::forward<V>(t) ...);
		}
		else {
			dbg_func(p, Tv<void>(1));
		}
	}
}

#define dbg(expr,...)  wws::dbg_choose< wws::Tv<decltype(expr)>::val >(#expr, wws::Tv<decltype(expr)>( (expr,##__VA_ARGS__) ) )