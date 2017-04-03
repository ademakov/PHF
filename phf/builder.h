#ifndef PERFECT_HASH_BUILDER_H
#define PERFECT_HASH_BUILDER_H

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
		nlevels_ = count;
#if PHF_DEBUG > 0
		std::array<std::size_t, count> level_ranks;
#endif

		for (std::size_t level = 0; level < count; level++) {
			if (keys_.empty()) {
				nlevels_ = level;
				break;
			}

			fill_level(level);

			std::size_t rank = 0;

			auto it = keys_.begin();
			auto size = level_bits_[level].size();
			while (it != keys_.end()) {
				hasher_ = *it;
				auto hash = hasher_[level];
				std::size_t index = hash % size;

				if (level_bits_[level][index]) {
#if PHF_DEBUG > 1
					if (size <= 128)
						std::cerr << *it << " - (" << index << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					keys_.erase(it++);
					++rank;
				} else {
#if PHF_DEBUG > 1
					if (size <= 128)
						std::cerr << *it << " + (" << index << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					++it;
				}
			}

#if PHF_DEBUG > 1
			if (size <= 128)
				std::cerr << "--\n";
			level_ranks[level] = rank;
#endif
		}

#if PHF_DEBUG > 0
		std::cerr << nlevels_ << ' ' << keys_.size() << '\n';
		for (std::size_t level = 0; level < nlevels_; level++) {
			std::cerr << level_ranks[level] << '/' << level_bits_[level].size()
				  << ' ';
		}
		std::cerr << '\n';
#endif

		std::size_t total_size = 0;
		std::array<std::size_t, count> sizes = {0};
		for (std::size_t level = 0; level < nlevels_; level++) {
			sizes[level] = level_bits_[level].size();
			total_size += sizes[level];
		}

		std::size_t bit_index = 0;
		std::vector<std::uint64_t> bitset((total_size + 63) / 64);
		for (std::size_t level = 0; level < nlevels_; level++) {
			for (std::size_t index = 0; index < sizes[level]; index++) {
				if (level_bits_[level][index]) {
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
		for (auto &bs : level_bits_)
			bs.clear();
		nlevels_ = 0;
	}

private:
	void fill_level(size_t level)
	{
		// Compute the required bitset size.
		std::size_t size = keys_.size() * gamma_;

		// Round it to a power of two but no less than 64.
		if (size < 64)
			size = 64;
		std::size_t nclear = __builtin_clzll((unsigned long long) (size - 1));
		size = size_t{1} << (8 * sizeof(unsigned long long) - nclear);

		level_bits_[level].clear();
		level_bits_[level].resize(size);
		std::vector<bool> collisions_(size);

		for (const auto &key : keys_) {
			hasher_ = key;

			auto hash = hasher_[level];
			std::size_t index = hash % size;
			if (collisions_[index])
				continue;

			if (not level_bits_[level][index]) {
				level_bits_[level][index] = true;
			} else {
				level_bits_[level][index] = false;
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

	std::size_t nlevels_ = 0;
	std::vector<bool> level_bits_[count];
};

} // namespace phf

#endif // PERFECT_HASH_BUILDER_H
