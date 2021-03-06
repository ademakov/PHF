#ifndef PERFECT_HASH_DETECT_H
#define PERFECT_HASH_DETECT_H

#include <type_traits>

#ifdef __has_include
// clang-format off
# if __has_include(<experimental/type_traits>)
#  include <experimental/type_traits>
# endif
// clang-format on
#endif

namespace phf {

//
// The std::experimental::is_detected_exact_v (Library Fundamentals v2)
// interface is used to check if a class defines a particular function.
//
// As this interface is only implemented by gcc 6.* provide a simplified
// version of this interface for others.
//
// To also support gcc 4.9 that does not support variable templates put
// them into struct templates.
//

#ifdef __cpp_lib_experimental_detect

template <typename Res, template <typename...> class Op, typename... Args>
struct detect
{
	static constexpr bool is_exact
		= std::experimental::is_detected_exact_v<Res, Op, Args...>;
};

#else

#if __GNUC__ < 5 && !defined __clang__

template <typename...>
struct voider
{
	using type = void;
};

template <typename... Ts>
using void_t = typename voider<Ts...>::type;

#else

template <typename...>
using void_t = void;

#endif

// Primary template handles all types not supporting the archetypal Op.
template <typename V, template <typename...> class Op, typename... Args>
struct detected_t
{
	using type = void;
};

// The specialization recognizes and handles only types supporting Op.
template <template <typename...> class Op, typename... Args>
struct detected_t<void_t<Op<Args...>>, Op, Args...>
{
	using type = Op<Args...>;
};

template <typename Res, template <typename...> class Op, typename... Args>
struct detect
{
	static constexpr bool is_exact
		= std::is_same<Res, typename detected_t<void, Op, Args...>::type>::value;
};

#endif

//
// Template utility that checks if a given type provides a hash operator
// required by the C++ standard library:
//
// std::size_t operator()(key_type)
//
template <typename H, typename K>
using standard_hasher_t = decltype(std::declval<H>()(std::declval<K>()));

//
// Template utility that checks if a given type provides a hash operator
// that accepts an additional seed argument:
//
// std::size_t operator()(key_type, integer_type)
//
template <typename H, typename K>
using extended_hasher_t = decltype(std::declval<H>()(std::declval<K>(), 1u));

template <typename H, typename K, typename V>
struct hasher_detect
{
	static constexpr bool is_standard = detect<V, standard_hasher_t, H, K>::is_exact;
	static constexpr bool is_extended = detect<V, extended_hasher_t, H, K>::is_exact;
};

} // namespace phf

#endif // PERFECT_HASH_DETECT_H
