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
	static constexpr std::uint64_t FNV1_64_INIT = UINT64_C(0xcbf29ce484222325);
	static constexpr std::uint64_t FNV_64_PRIME = UINT64_C(0x100000001b3);

	std::size_t operator()(string_view data, std::uint64_t hval)
	{
		auto *bp = reinterpret_cast<const unsigned char *>(data.data());
		auto *ep = reinterpret_cast<const unsigned char *>(data.data()) + data.size();
		while (bp < ep) {
			hval ^= *bp++;
			hval *= FNV_64_PRIME;
		}
		return hval;
	}
};

} // namespace public_suffix

#endif // PUBLIC_SUFFIX_TYPES_H
