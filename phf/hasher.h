#ifndef PERFECT_HASH_HASHER_H
#define PERFECT_HASH_HASHER_H

#include <algorithm>
#include <array>
#include <bitset>
#include <functional>
#include <iostream>
#include <utility>

#include "detect.h"
#include "rng.h"

namespace phf {

//
// A hasher that produces multiple hash values based on a standard or
// extended hasher.
//
template <std::size_t N, typename Key, typename Hash = std::hash<Key>>
class hasher : private Hash
{
	// Workarounds for gcc 4.9 where std::min() and std::max() are const
	// rather than constexpr despite the -std=c++14 compiler flag.
	template <typename T>
	static constexpr T min(T a, T b)
	{
		return a < b ? a : b;
	}
	template <typename T>
	static constexpr T max(T a, T b)
	{
		return a > b ? a : b;
	}

public:
	using key_type = Key;
	using base_hasher = Hash;
	using result_type = typename base_hasher::result_type;

	static constexpr std::size_t min_count = 2;
	static constexpr std::size_t max_count = 256;
	static constexpr std::size_t count = min(max(N, min_count), max_count);

	using seed_type = rng::rng128::result_type;
	using seed_array_type = std::array<seed_type, count>;

	hasher(seed_type seed = 1)
	{
		rng::rng128 rng(seed);
		for (auto &s : seeds_)
			s = rng();
	}

	hasher(const seed_array_type &seeds) : seeds_(seeds)
	{
	}

	hasher(const hasher &other) : seeds_(other.seeds_)
	{
	}

	void operator=(const key_type &key)
	{
		key_ = key;
	}

	result_type operator[](std::size_t index)
	{
		return hash(key_, seeds_[index]);
	}

	const seed_array_type &seeds() const
	{
		return seeds_;
	}

private:
	template <typename H = base_hasher, typename K = key_type, typename V = result_type,
		  std::enable_if_t<hasher_detect<H, K, V>::is_extended, int> = 0>
	result_type hash(const key_type &key, seed_type seed)
	{
		return base_hasher::operator()(key, seed);
	}

	template <typename H = base_hasher, typename K = key_type, typename V = result_type,
		  std::enable_if_t<not hasher_detect<H, K, V>::is_extended, int> = 0>
	result_type hash(const key_type &key, seed_type seed)
	{
		// TODO: Do something better.
		return base_hasher::operator()(key) * seed;
	}

	// The current key to hash.
	key_type key_;

	// Seeds for hash functions.
	seed_array_type seeds_;
};

//
// A hasher that produces on demand and remembers multiple hash values
// based on a standard or extended hasher.
//
template <std::size_t N, typename Key, typename Hash = std::hash<Key>>
class caching_hasher : private hasher<N, Key, Hash>
{
public:
	using base = hasher<N, Key, Hash>;
	using typename base::key_type;
	using typename base::base_hasher;
	using typename base::result_type;
	using typename base::random_type;
	using base::count;

	caching_hasher(random_type seed = 1) : base(seed)
	{
	}

	caching_hasher(const caching_hasher &other) : base(other)
	{
	}

	void operator=(const key_type &key)
	{
		base::operator=(key);
		isset_.reset();
	}

	result_type operator[](std::size_t index)
	{
		if (!isset_[index]) {
			isset_.set(index);
			values_[index] = base::operator[](index);
		}
		return values_[index];
	}

private:
	// Cached hash values computed on demand.
	std::bitset<count> isset_;
	std::array<result_type, count> values_;
};

} // namespace phf

#endif // PERFECT_HASH_HASHER_H
