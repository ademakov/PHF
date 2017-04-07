#ifndef PERFECT_HASH_BUILDER_H
#define PERFECT_HASH_BUILDER_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

#include "hasher.h"
#include "mph.h"

namespace phf {

template <std::size_t N, typename Key, typename Hash = std::hash<Key>>
class builder
{
public:
	using key_type = Key;
	using base_hasher_type = Hash;
	using hasher_type = hasher<N, key_type, base_hasher_type>;

	static constexpr std::size_t count = hasher_type::count;

	using mph_type = minimal_perfect_hash<count, key_type, base_hasher_type>;

	builder(double gamma, std::uint64_t seed) : gamma_(gamma), seed_(seed), hasher_(seed)
	{
	}

	void insert(const key_type &key)
	{
		keys_.insert(key);
	}

	std::unique_ptr<mph_type> build()
	{
		std::size_t nlevels = count;
		std::vector<bool> level_bits[count];

		// Prepare a key filter.
		std::vector<bool> filter;
		filter.resize(power_of_two(keys_.size() * 2));

#if PHF_DEBUG > 0
		std::array<std::size_t, count> level_ranks{{0}};
		std::array<std::size_t, count> level_conflicts{{0}};
#endif
		for (std::size_t level = 0; level < count; level++) {
			if (keys_.empty()) {
				nlevels = level;
				break;
			}

			// Find a conflict-free key set.
			fill_level(level, level_bits[level]);

			auto it = keys_.begin();
			auto size = level_bits[level].size();
			while (it != keys_.end()) {
				hasher_ = *it;
				auto hash = hasher_[level];
				std::size_t index = hash & (size - 1);

				if (level_bits[level][index]) {
					keys_.erase(it++);
#if PHF_DEBUG > 0
					level_ranks[level]++;
#endif
				} else {
					++it;

					// Mark a conflicting key in the filter.
					if (level < 2) {
						index = hash & (filter.size() - 1);
						filter[index] = true;
					}
#if PHF_DEBUG > 0
					level_conflicts[level]++;
#endif
				}
			}
		}

#if PHF_DEBUG > 0
		std::cerr << nlevels << ' ' << keys_.size() << '\n';
		for (std::size_t level = 0; level < nlevels; level++) {
			std::cerr << level_ranks[level] << '/' << level_conflicts[level] << '/'
				  << level_bits[level].size() << ' ';
		}
		std::cerr << '\n';
#endif

		std::size_t total_size = 0;
		std::array<std::size_t, count> sizes{{0}};
		for (std::size_t level = 0; level < nlevels; level++) {
			sizes[level] = level_bits[level].size();
			total_size += sizes[level];
		}
		if (nlevels > 1) {
			total_size += filter.size();
		}

		std::size_t bit_index = 0;
		std::vector<std::uint64_t> bitset((total_size + 63) / 64);
		for (std::size_t level = 0; level < nlevels; level++) {
			for (std::size_t index = 0; index < sizes[level]; index++) {
				if (level_bits[level][index]) {
					auto mask = UINT64_C(1) << (bit_index % 64);
					bitset[bit_index / 64] |= mask;
				}
				bit_index++;
			}
		}
		if (nlevels > 1) {
			for (std::size_t index = 0; index < filter.size(); index++) {
				if (filter[index]) {
					auto mask = UINT64_C(1) << (bit_index % 64);
					bitset[bit_index / 64] |= mask;
				}
				bit_index++;
			}
		}

		auto result = std::make_unique<mph_type>(hasher_, sizes, std::move(bitset));
		for (const auto &key : keys_)
			result->insert(key);

		return result;
	}

	void clear()
	{
		hasher_ = hasher_type(seed_);
		keys_.clear();
	}

private:
	std::size_t power_of_two(std::size_t n)
	{
		unsigned long long s = n ? n : 2;
		std::size_t nbits = (8 * sizeof(s) - __builtin_clzll(n - 1));
		return (size_t{1} << nbits);
	}

	void fill_level(size_t level, std::vector<bool> &bitset)
	{
		// Compute the required bitset size.
		std::size_t size = keys_.size() * gamma_;
		// Round it to a power of two but no less than 64.
		size = power_of_two(std::max(size, std::size_t{64}));

		bitset.clear();
		bitset.resize(size);
		std::vector<bool> collisions_(size);

		for (const auto &key : keys_) {
			hasher_ = key;

			auto hash = hasher_[level];
			std::size_t index = hash & (size - 1);
#if PHF_DEBUG > 2
			std::cerr << std::hex << hash << ' ' << (size - 1) << ' ' << index
				  << std::dec << '\n';
#endif
			if (collisions_[index])
				continue;

			if (not bitset[index]) {
				bitset[index] = true;
			} else {
				bitset[index] = false;
				collisions_[index] = true;
			}
		}
	}

	// The gamma parameter gamma specifies how many bits per key are
	// allocated on a given bitset level.
	const double gamma_;

	const std::uint64_t seed_;
	hasher_type hasher_;

	std::unordered_set<key_type> keys_;
};

} // namespace phf

#endif // PERFECT_HASH_BUILDER_H
