#include<cstdlib>

namespace wws {
	template<typename T>
	T *aligned_alloc(size_t alignment, size_t size)
	{
#if defined(WIN32)
		return (T *)_aligned_malloc(size, alignment);
#elif defined(__linux__)
		return (T *)std::aligned_alloc(alignment, size);
#endif
	}

	void aligned_free(void *ptr)
	{
#if defined(WIN32)
		return _aligned_free(ptr);
#elif defined(__linux__)
		return std::free(ptr);
#endif
	}

	template<typename T>
	struct arr_len;

	template<typename T,size_t N>
	struct arr_len<T[N]>
	{
		const static size_t val = N;
	};

	template<typename T, size_t N>
	struct arr_len<T(&)[N]>
	{
		const static size_t val = N;
	};

	template<typename T>
	size_t arr_len_v = arr_len<T>::val;

	template<typename Ret,typename T>
	constexpr Ret arrLen(T&& t)
	{
		return static_cast<Ret>(arr_len_v<T>);
	}

    template<typename T>
    constexpr size_t arrLen(T&& t)
    {
        return arr_len_v<T>;
    }
}