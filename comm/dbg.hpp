#pragma once

#include <iostream>
#include <utility>
#include <tuple>

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

	struct V{

	};

	template<typename T, bool Not_Last_One>
	void check_and_cout(std::ostream& out, T&& t)
	{
		if constexpr (can_cout<T>::val)
		{
			out << t;
		}
		else {
			out << "Can not print!";
		}
		if constexpr (Not_Last_One)
		{
			out << ",";
		}
	}

	template<typename Tup, size_t ... Idx>
	void go_through_cout(std::ostream& out, Tup&& tup, std::index_sequence<Idx...> is)
	{
		(check_and_cout< decltype(std::get<Idx>(tup)), (Idx < (std::tuple_size_v<Tup> -1)) >(out, std::get<Idx>(tup)), ...);
	}

	template<typename ...Args>
	std::ostream& operator<<(std::ostream& out, std::tuple<Args...>&& tup)
	{
		out << "( ";
		if constexpr (std::tuple_size_v<std::tuple<Args...>> > 0)
		{
			go_through_cout(out, std::forward<decltype(tup)>(tup), std::make_index_sequence< std::tuple_size_v<std::tuple<Args...>> >());
		}
		out << " )";
		return out;
	}

	template<typename T>
	auto dbg_func(const char *expr,const char *file,int line, T&& t) -> T
	{
		if constexpr (can_cout<T>::val)
		{
			std::cout << "[" << file  << ":" << line << "] " << expr << " = " << t << std::endl;
		}
		else {
			std::cout << "[" << file  << ":" << line << "] " << expr << " = " << "This type can not print!!!" << std::endl;
		}
		return std::forward<T>(t);
	}

    template<typename T = V>
    auto dbg_func(const char *expr,const char *file,int line, V&& t) -> void
    {
        std::cout << "[" << file  << ":" << line << "] "<< expr << " = " << "void" << std::endl;
        return;
    }

	template<typename... Args>
	auto dbg_func(const char *expr, const char *file, int line, std::tuple<Args...> &&tup) -> std::tuple<Args...>
	{
		std::cout << "[" << file << ":" << line << "] " << expr << " = " << std::forward<std::tuple<Args...>>(tup) << std::endl;
		return std::forward<std::tuple<Args...>>(tup);
	}
}

#define dbg(...)  wws::dbg_func(#__VA_ARGS__,__FILE__,__LINE__,(__VA_ARGS__))
#define dbgv(...) wws::dbg_func(#__VA_ARGS__,__FILE__,__LINE__,wws::V())
