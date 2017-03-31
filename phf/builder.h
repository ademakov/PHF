#ifndef PERFECT_HASH_BUILDER_H
#define PERFECT_HASH_BUILDER_H

#include <functional>
#include <unordered_set>
#include <vector>

#include "hasher.h"

namespace phf {

template <std::size_t N, typename K, typename H>
class builder
{
public:
	using key_type = K;
	using base_hasher_type = H;
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
		nbitsets_ = count;

		for (std::size_t index = 0; index < count; index++) {
			if (keys_.empty()) {
				nbitsets_ = index;
				break;
			}

			fill_bitset(index);

			std::size_t rank = 0;

			auto it = keys_.begin();
			auto size = bitsets_[index].size();
			while (it != keys_.end()) {
				hasher_ = *it;
				auto hash = hasher_[index];
				std::size_t bit = bit_index(hash, size);

				if (bitsets_[index][bit]) {
#if DEBUG
					if (size <= 64)
						std::cout << *it << " - (" << bit << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					keys_.erase(it++);
					++rank;
				} else {
#if DEBUG
					if (size <= 64)
						std::cout << *it << " + (" << bit << ' '
							  << std::hex << hash << std::dec
							  << ")\n";
#endif

					++it;
				}
			}

#if DEBUG
			if (size <= 64)
				std::cout << "--\n";
#endif

			ranks_[index] = rank;
		}

		std::cout << nbitsets_ << ' ' << keys_.size() << '\n';
		for (std::size_t index = 0; index < nbitsets_; index++) {
			std::cout << ranks_[index] << '/' << bitsets_[index].size() << ' ';
		}
		std::cout << '\n';
	}

	void clear()
	{
		hasher_ = hasher_type(seed_);

		keys_.clear();
		for (auto &bs : bitsets_)
			bs.clear();
		nbitsets_ = 0;
	}

private:
	std::size_t bit_index(hash_type hash, std::size_t size)
	{
		return hash % size;
	}

	void fill_bitset(size_t index)
	{
		// Choose the bitset size twice the number of keys rounding
		// it to a 64-bit multiple.
		std::size_t size = keys_.size() * gamma_;
		size = (size + 63) & ~63;

		bitsets_[index].clear();
		bitsets_[index].resize(size);
		std::vector<bool> collisions_(size);

		for (const auto &key : keys_) {
			hasher_ = key;

			auto hash = hasher_[index];
			std::size_t bit = bit_index(hash, size);
			if (collisions_[bit])
				continue;

			if (not bitsets_[index][bit]) {
				bitsets_[index][bit] = true;
			} else {
				bitsets_[index][bit] = false;
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
	std::vector<bool> bitsets_[count];
	std::size_t nbitsets_ = 0;

	std::array<std::size_t, count> ranks_;
};

} // namespace phf

#endif // PERFECT_HASH_BUILDER_H
