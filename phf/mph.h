#ifndef PERFECT_HASH_MINIMAL_PERFECT_H
#define PERFECT_HASH_MINIMAL_PERFECT_H

#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "hasher.h"

namespace phf {

static constexpr std::size_t not_found = (unsigned int) (-1);

//
// A minimal perfect hash function object.
//
template <std::size_t N, typename Key, typename Hash = std::hash<Key>,
	  typename Rank = std::size_t, typename Bitset = std::vector<std::uint64_t>,
	  bool enable_extra_keys = true>
class minimal_perfect_hash
{
public:
	using key_type = Key;
	using rank_type = Rank;
	using base_hasher_type = Hash;
	using hasher_type = hasher<N, key_type, base_hasher_type>;
	using bitset_type = Bitset;

	static constexpr rank_type count = hasher_type::count;

	using bitset_value_type = typename bitset_type::value_type;
	static_assert(sizeof(bitset_value_type) == 8, "invalid value type");

	static constexpr rank_type value_nbits = 8 * sizeof(bitset_value_type);
	static constexpr rank_type block_nvalues = 4;
	static constexpr rank_type block_nbits = value_nbits * block_nvalues;

	minimal_perfect_hash(const hasher_type &hasher, std::array<rank_type, count> levels,
			     bitset_type &&bitset)
		: hasher_(hasher), levels_(levels), bitset_(std::move(bitset)), filter_(0),
		  filter_size_(0), max_rank_(0)
	{
		rank_type rank_space = 0;
		for (auto level : levels_) {
			if (level != 0 && (level < value_nbits || (level & (level - 1)) != 0))
				throw std::invalid_argument("each level must be a power of 2");
			rank_space += level;
		}

		// Check if there is a conflict filter.
		rank_type total_space = bitset_.size() * value_nbits;
		if (rank_space < total_space) {
			filter_ = rank_space / value_nbits;
			filter_size_ = total_space - rank_space;
		}

		// Initialize cumulative rank count array.
		size_t nblocks = (rank_space + block_nbits - 1) / block_nbits;
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

	std::size_t operator[](const key_type &key) const
	{
		hasher_ = key;

		rank_type base = 0;
		for (std::size_t level = 0; level < count; level++) {
			auto size = levels_[level];
			if (size == 0)
				continue;

			auto hash = hasher_[level];
			auto bit_index = base + (hash & (size - 1));

			auto index = bit_index / value_nbits;
			auto shift = bit_index % value_nbits;
			auto value = bitset_[index];
			auto mask = UINT64_C(1) << shift;
			if ((value & mask) != 0)
				return get_rank(index, value, mask);

			if (level < 2 && filter_size_) {
				bit_index = hash & (filter_size_ - 1);
				index = bit_index / value_nbits;
				shift = bit_index % value_nbits;
				mask = UINT64_C(1) << shift;
				if ((bitset_[filter_ + index] & mask) == 0)
					return not_found;
			}

			base += size;
		}

		if (enable_extra_keys && !extra_keys_.empty()) {
			auto it = extra_keys_.find(key);
			if (it != extra_keys_.end())
				return it->second;
		}

		return not_found;
	}

	void emit(std::ostream &os, const std::string &name, const std::string &key_type_name,
		  const std::string &hasher_type_name)
	{
		// Find out how many levels are really needed.
		std::size_t required_count = count;
		while (required_count > 1 && levels_[required_count - 1] == 0)
			--required_count;

		std::string emit_count = std::to_string(required_count);
		std::string emit_class = "phf::minimal_perfect_hash<";
		emit_class += emit_count + ", " + key_type_name + ", " + hasher_type_name;
		emit_class += ", std::size_t, static_bitset, ";
		emit_class += extra_keys_.empty() ? "false>" : "true>";

		os << "namespace " << name << " {\n\n";
		os << "static constexpr std::size_t static_bitset_size = " << bitset_.size()
		   << ";\n\n";
		os << "phf::hasher<" << emit_count << ", " << key_type_name << ", "
		   << hasher_type_name << "> static_hasher(std::array<std::uint64_t, "
		   << emit_count << "> {{\n\t0x" << std::hex;
		for (std::size_t i = 0; i < required_count; i++) {
			os << hasher_.seeds()[i];
			if (i != (required_count - 1))
				os << ", 0x";
		}
		os << std::dec << "\n}});\n\n";
		os << "std::array<std::size_t, " << emit_count << "> static_levels {{\n\t";
		for (std::size_t i = 0; i < required_count; i++)
			os << levels_[i] << ", ";
		os << "\n}};\n\n";
		os << "std::array<std::uint64_t, static_bitset_size> static_bitset_data {{\n";
		for (auto value : bitset_)
			os << "\t0x" << std::hex << value << std::dec << ",\n";
		os << "}};\n\n";
		os << "struct static_bitset {\n";
		os << "\tusing value_type = std::uint64_t;\n";
		os << "\tusing iterator = std::uint64_t *;\n";
		os << "\tusing const_iterator = const std::uint64_t *;\n";
		os << "\tstd::size_t size() { return static_bitset_data.size(); }\n";
		os << "\tvalue_type& operator[](std::size_t i) { return static_bitset_data[i]; "
		      "}\n";
		os << "\tconst value_type& operator[](std::size_t i) const { return "
		      "static_bitset_data[i]; }\n";
		os << "};\n\n";
		os << "struct mph : " << emit_class << " {\n";
		os << "\tmph() : " << emit_class
		   << "(static_hasher, static_levels, static_bitset())"
		   << " {\n";
		os << "\t}\n";
		os << "} instance;\n\n";
		os << "} // namespace " << name << "\n\n";
	}

private:
	mutable hasher_type hasher_;
	std::array<rank_type, count> levels_;
	bitset_type bitset_;

	rank_type filter_;
	rank_type filter_size_;

	rank_type max_rank_;
	std::vector<rank_type> block_ranks_;
	std::unordered_map<key_type, rank_type> extra_keys_;

	std::size_t get_rank(std::size_t index, std::uint64_t value, std::uint64_t mask) const
	{
		rank_type rank = block_ranks_[index / block_nvalues];
		switch (index % block_nvalues) {
		case 3:
			rank += __builtin_popcountll(bitset_[index - 3]);
		// no break
		case 2:
			rank += __builtin_popcountll(bitset_[index - 2]);
		// no break
		case 1:
			rank += __builtin_popcountll(bitset_[index - 1]);
			// no break
		case 0:
			rank +=  __builtin_popcountll(value & (mask - 1));
		}
		return rank;
	}
};

} // namespace phf

#endif // PERFECT_HASH_MINIMAL_PERFECT_H
