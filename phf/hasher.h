#ifndef PERFECT_HASH_HASHER_H
#define PERFECT_HASH_HASHER_H

#include <algorithm>
#include <array>
#include <bitset>
#include <functional>
#include <utility>

#include "detect.h"
#include "random.h"

namespace phf {

//
// A hasher that produces multiple hash values based on a standard or
// extended hasher.
//
template <std::size_t N, typename Key, typename Hash = std::hash<Key>>
class hasher : private Hash
{
public:
	using key_type = Key;
	using base_hasher = Hash;

	using result_type = std::size_t;
	using random_type = rng128::result_type;

	static constexpr std::size_t min_count = 2;
	static constexpr std::size_t max_count = 256;
	static constexpr std::size_t count = std::min(std::max(N, min_count), max_count);

	hasher(random_type seed = 1)
	{
		rng128 rng(seed);
		for (auto &s : seeds_)
			s = rng();
	}

	void operator=(const key_type &key)
	{
		key_ = key;
		isset_.reset();
	}

	result_type operator[](std::size_t index)
	{
		if (!isset_[index])
			compute(index);
		return values_[index];
	}

private:
	template <typename H = base_hasher, typename K = key_type,
		  typename std::enable_if_t<is_extended_hasher_v<H, K>, int> = 0>
	void compute(std::size_t index) {
		isset_.set(index);
		values_[index] = base_hasher::operator()(key_, seeds_[index]);
	}

	template <typename H = base_hasher, typename K = key_type,
		  typename std::enable_if_t<not is_extended_hasher_v<H, K>, int> = 0>
	void compute(std::size_t index) {
		isset_.set(index);
		// TODO: Do something better.
		values_[index] = base_hasher::operator()(key_) * seeds_[index];
	}

	// The current key to hash.
	key_type key_;

	// Seeds for hash functions.
	std::array<random_type, count> seeds_;

	// Hash values computed on demand.
	std::bitset<count> isset_;
	std::array<result_type, count> values_;
};

} // namespace phf

#endif // PERFECT_HASH_HASHER_H
