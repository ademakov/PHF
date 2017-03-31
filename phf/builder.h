#ifndef PERFECT_HASH_BUILDER_H
#define PERFECT_HASH_BUILDER_H

#include <unordered_set>
#include <vector>

#include "hasher.h"

namespace phf {

template <std::size_t N, typename Key, typename Hash = std::hash<Key>>
class builder
{
public:
	using key_type = Key;
	using base_hasher_type = Hash;
	using hasher_type = hasher<N, key_type, base_hasher_type>;
	using hash_type = typename hasher_type::result_type;

	static constexpr std::size_t count = hasher_type::count;

	builder(double gamma, std::uint64_t seed) : gamma_(gamma), seed_(seed), hasher_(seed)
	{
	}

	void insert(const key_type &key)
	{
		keys_.insert(key);
	}

	void build()
	{
		nlevels_ = count;

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
				std::size_t bit = bit_index(hash, size);

				if (level_bits_[level][bit]) {
#if PHF_DEBUG > 1
					if (size <= 128)
						std::cout << *it << " - (" << bit << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					keys_.erase(it++);
					++rank;
				} else {
#if PHF_DEBUG > 1
					if (size <= 128)
						std::cout << *it << " + (" << bit << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					++it;
				}
			}

#if PHF_DEBUG > 1
			if (size <= 128)
				std::cout << "--\n";
#endif

			level_ranks_[level] = rank;
		}

#if PHF_DEBUG > 0
		std::cout << nlevels_ << ' ' << keys_.size() << '\n';
		for (std::size_t level = 0; level < nlevels_; level++) {
			std::cout << level_ranks_[level] << '/' << level_bits_[level].size()
				  << ' ';
		}
		std::cout << '\n';
#endif
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
	std::size_t bit_index(hash_type hash, std::size_t size)
	{
		return hash % size;
	}

	void fill_level(size_t level)
	{
		// Choose the bitset size twice the number of keys rounding
		// it to a 64-bit multiple.
		std::size_t size = keys_.size() * gamma_;
		size = (size + 63) & ~63;

		level_bits_[level].clear();
		level_bits_[level].resize(size);
		std::vector<bool> collisions_(size);

		for (const auto &key : keys_) {
			hasher_ = key;

			auto hash = hasher_[level];
			std::size_t bit = bit_index(hash, size);
			if (collisions_[bit])
				continue;

			if (not level_bits_[level][bit]) {
				level_bits_[level][bit] = true;
			} else {
				level_bits_[level][bit] = false;
				collisions_[bit] = true;
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
	std::array<std::size_t, count> level_ranks_;
};

} // namespace phf

#endif // PERFECT_HASH_BUILDER_H
