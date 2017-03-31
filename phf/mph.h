#ifndef PERFECT_HASH_MINIMAL_PERFECT_H
#define PERFECT_HASH_MINIMAL_PERFECT_H

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "hasher.h"

namespace phf {

//
// A minimal perfect hash function object.
//
template <std::size_t N, typename Key, typename Hash = std::hash<Key>,
	  typename Rank = std::size_t, typename Bitset = std::vector<std::uint64_t>>
class minimal_perfect_hash
{
public:
	using key_type = Key;
	using rank_type = Rank;
	using base_hasher_type = Hash;
	using hasher_type = hasher<N, key_type, base_hasher_type>;
	using bitset_type = Bitset;
	using bitset_value_type = typename bitset_type::value_type;

	static constexpr rank_type count = hasher_type::count;

	static constexpr rank_type not_found = -1;

	static constexpr rank_type block_nbits = 256;
	static constexpr rank_type value_nbits = 8 * sizeof(bitset_value_type);
	static constexpr rank_type block_nvalues = block_nbits / value_nbits;

	static_assert(value_nbits == 64);

	minimal_perfect_hash(const hasher_type &hasher, std::array<rank_type, count> levels,
			     bitset_type &&bitset)
		: hasher_(hasher), levels_(levels), bitset_(std::move(bitset)), max_rank_(0)
	{
		size_t nblocks = (bitset_.size() * value_nbits + block_nbits - 1) / block_nbits;
		block_ranks_.resize(nblocks);

		for (rank_type b = 0; b < nblocks; b++) {
			block_ranks_[b] = max_rank_;
			for (rank_type v = 0; v < block_nvalues; v++) {
				rank_type i = b * block_nvalues + v;
				max_rank_ += __builtin_popcountll(bitset_[i]);
			}
		}
	}

	rank_type insert(const key_type &key)
	{
		auto rank = operator[](key);
		if (rank == not_found) {
			rank = max_rank_++;
			extra_keys_.emplace(std::make_pair(key, rank));
#if PHF_DEBUG > 0
			std::cerr << "extra rank " << rank << " for key " << key << '\n';
#endif
		}
		return rank;
	}

	rank_type size() const
	{
		return max_rank_;
	}

	rank_type operator[](const key_type &key) const
	{
		hasher_ = key;

		rank_type base = 0;
		for (std::size_t level = 0; level < count; level++) {
			auto hash = hasher_[level];
			auto size = levels_[level];
			auto bit_index = base + hash % size;

			auto index = bit_index / value_nbits;
			auto shift = bit_index % value_nbits;
			auto value = bitset_[index];
			auto mask = UINT64_C(1) << shift;
			if ((value & mask) != 0) {
				auto block = bit_index / block_nbits;
				auto block_index = block * block_nvalues;

				rank_type rank = __builtin_popcountll(value & (mask - 1));
				for (; block_index < index; block_index++)
					rank += __builtin_popcountll(bitset_[block_index]);
				return block_ranks_[block] + rank;
			}

			base += size;
		}

		auto it = extra_keys_.find(key);
		if (it == extra_keys_.end())
			return not_found;
		return it->second;
	}

private:
	mutable hasher_type hasher_;
	std::array<rank_type, count> levels_;
	bitset_type bitset_;

	std::vector<rank_type> block_ranks_;
	rank_type max_rank_;
	std::unordered_map<key_type, rank_type> extra_keys_;
};

} // namespace phf

#endif // PERFECT_HASH_MINIMAL_PERFECT_H
