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
	using bitset_value_type = typename bitset_type::value_type;

	static constexpr rank_type count = hasher_type::count;

	static constexpr rank_type block_nbits = 256;
	static constexpr rank_type value_nbits = 8 * sizeof(bitset_value_type);
	static constexpr rank_type block_nvalues = block_nbits / value_nbits;

	static_assert(value_nbits == 64, "invalid value type");

	minimal_perfect_hash(const hasher_type &hasher, std::array<rank_type, count> levels,
			     bitset_type &&bitset)
		: hasher_(hasher), levels_(levels), bitset_(std::move(bitset)), max_rank_(0)
	{
		for (auto level : levels_) {
			if (level != 0 && (level & (level - 1)) != 0)
				throw std::invalid_argument("each level must be a power of 2");
		}

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
			base += size;

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
		   << emit_count << ">{0x" << std::hex;
		for (std::size_t i = 0; i < required_count; i++) {
			os << hasher_.seeds()[i];
			if (i != (required_count - 1))
				os << ", 0x";
		}
		os << std::dec << "});\n\n";
		os << "std::array<std::size_t, " << emit_count << "> static_levels = {";
		for (std::size_t i = 0; i < required_count; i++)
			os << levels_[i] << ", ";
		os << "};\n\n";
		os << "std::array<std::uint64_t, static_bitset_size> static_bitset_data = {\n";
		for (auto value : bitset_)
			os << "\t0x" << std::hex << value << std::dec << ",\n";
		os << "};\n\n";
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

	std::vector<rank_type> block_ranks_;
	rank_type max_rank_;
	std::unordered_map<key_type, rank_type> extra_keys_;
};

} // namespace phf

#endif // PERFECT_HASH_MINIMAL_PERFECT_H
