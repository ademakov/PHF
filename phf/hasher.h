#ifndef PERFECT_HASH_HASHER_H
#define PERFECT_HASH_HASHER_H

#include <algorithm>
#include <array>
#include <bitset>
#include <functional>
#include <iostream>
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

	using seeds_array = std::array<random_type, count>;

	hasher(random_type seed = 1)
	{
		rng128 rng(seed);
		for (auto &s : seeds_)
			s = rng();
	}

	hasher(const seeds_array &seeds) : seeds_(seeds)
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

	const seeds_array &seeds() const
	{
		return seeds_;
	}

private:
	template <typename H = base_hasher, typename K = key_type,
		  std::enable_if_t<is_extended_hasher_v<H, K>, int> = 0>
	result_type hash(const key_type &key, random_type seed)
	{
		return base_hasher::operator()(key, seed);
	}

	template <typename H = base_hasher, typename K = key_type,
		  std::enable_if_t<not is_extended_hasher_v<H, K>, int> = 0>
	result_type hash(const key_type &key, random_type seed)
	{
		// TODO: Do something better.
		return base_hasher::operator()(key) * seed;
	}

	// The current key to hash.
	key_type key_;

	// Seeds for hash functions.
	seeds_array seeds_;
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
