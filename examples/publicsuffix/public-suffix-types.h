#ifndef PUBLIC_SUFFIX_TYPES_H
#define PUBLIC_SUFFIX_TYPES_H

#include <cstdint>
#include <iostream>

// clang-format off
#ifdef __has_include
# if __has_include(<string_view>)
#  include <string_view>
#  define has_string_view		1
# endif
#endif
#if !has_string_view
# include <experimental/string_view>
#endif
// clang-format on

#include "MurmurHash3.h"
#include "Spooky.h"

namespace public_suffix {

#if has_string_view
using string_view = std::string_view;
#else
using string_view = std::experimental::string_view;
#endif

enum class Rule : uint8_t {
	kDefault,
	kRegular,
	kException,
};

static inline std::ostream &
operator<<(std::ostream &os, Rule rule)
{
	const char *cp = nullptr;
	switch (rule) {
	case Rule::kDefault:
		cp = "Rule::kDefault";
		break;
	case Rule::kRegular:
		cp = "Rule::kRegular";
		break;
	case Rule::kException:
		cp = "Rule::kException";
		break;
	}
	os << cp;
	return os;
}

struct Fnv64
{
	using result_type = std::uint64_t;

	static constexpr std::uint64_t FNV1_64_INIT = UINT64_C(0xcbf29ce484222325);
	static constexpr std::uint64_t FNV_64_PRIME = UINT64_C(0x100000001b3);

	result_type operator()(string_view data, std::uint64_t hval)
	{
		auto *bp = reinterpret_cast<const unsigned char *>(data.data());
		auto *ep = reinterpret_cast<const unsigned char *>(data.data()) + data.size();
		while (bp < ep) {
			hval ^= *bp++;
			hval *= FNV_64_PRIME;
		}
		return hval;
	}

	result_type operator()(string_view data)
	{
		return operator()(data, FNV1_64_INIT);
	}
};

struct Murmur
{
	using result_type = std::uint64_t;

	result_type operator()(string_view data, std::uint64_t seed)
	{
		std::uint64_t out[2];
		MurmurHash3_x64_128(data.data(), data.size(), seed, out);
		return out[0];
	}
};

struct Spooky
{
	using result_type = std::uint64_t;

	result_type operator()(string_view data, std::uint64_t seed)
	{
		return SpookyHash::Hash64(data.data(), data.size(), seed);
	}
};

} // namespace public_suffix

#endif // PUBLIC_SUFFIX_TYPES_H
