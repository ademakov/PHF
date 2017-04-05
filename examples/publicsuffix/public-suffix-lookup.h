#ifndef PUBLIC_SUFFIX_LOOKUP_H
#define PUBLIC_SUFFIX_LOOKUP_H

#include "public-suffix-types.h"

// This header is generated.
#include "public-suffix-tables.h"

#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)

namespace public_suffix {

using namespace phf;

static inline Node *
lookup_second_level(string_view label)
{
	auto rank = second_level_index::instance[label];
	if (rank == phf::not_found)
		return nullptr;

	Node *node = &second_level_nodes[rank];
	if (label != string_view(node->label, node->label_len))
		return nullptr;

	return node;
}

static inline Node *
lookup_next_level(Node *node, string_view label)
{
	if (node->size == 0)
		return nullptr;

	auto begin = node->node;
	auto end = node->node + node->size;
	auto it = std::find_if(begin, end, [label](Node &x) {
		return label == string_view(x.label, x.label_len);
	});
	if (it == end)
		return nullptr;

	return it;
}

static inline string_view
lookup(string_view name)
{
	if (name.size() > std::numeric_limits<std::uint16_t>::max())
		throw std::runtime_error("too long domain name");

	// Count the dots and remember the last 4 of them.
	std::size_t num_dots = 0;
	union {
		std::uint64_t four;
		std::uint16_t pos[4];
	} dots = {0};
	const char *s = name.data();
	for (std::size_t i = 0; i < name.size(); i++) {
		if (s[i] == '.') {
			dots.four = i | (dots.four << 16);
			num_dots++;
		}
	}

	// Check if the domain name contains no dots.
	if (unlikely(num_dots == 0))
		return name;

	// Check if the domain name contains exactly one dot.
	if (num_dots == 1) {
		Node *level_2 = lookup_second_level(name);
		if (level_2) {
			if (level_2->rule == Rule::kException)
				return name.substr(dots.pos[0] + 1);
			if (level_2->rule == Rule::kRegular)
				return name;
		}

		auto label_1 = name.substr(dots.pos[0] + 1);
		return lookup_first(label_1) ? name : label_1;
	}

	// Handle the most likely case of domains with up to 3 dots (that is
	// up to 4 labels).
	if (likely(num_dots <= 3)) {
		auto label_2 = name.substr(dots.pos[1] + 1);
		auto level_2 = lookup_second_level(label_2);
		if (level_2) {
			auto start_3 = num_dots == 2 ? 0 : dots.pos[2] + 1;
			auto label_3 = name.substr(start_3, dots.pos[1] - start_3);
			auto level_3 = lookup_next_level(level_2, label_3);
			if (level_3) {
				if (num_dots == 3) {
					auto label_4 = name.substr(0, dots.pos[2]);
					auto level_4 = lookup_next_level(level_3, label_4);
					if (level_4) {
						if (level_4->rule == Rule::kException)
							return name.substr(start_3);
						if (level_4->rule == Rule::kRegular)
							return name;
					}

					if (level_3->wildcard)
						return name;
				}

				if (level_3->rule == Rule::kException)
					return label_2;
				if (level_3->rule == Rule::kRegular)
					return name.substr(start_3);
			}

			if (level_2->wildcard)
				return name.substr(start_3);
			if (level_2->rule == Rule::kException)
				return name.substr(dots.pos[0] + 1);
			if (level_2->rule == Rule::kRegular)
				return label_2;
		}

		auto label_1 = name.substr(dots.pos[0] + 1);
		return lookup_first(label_1) ? label_2 : label_1;
	}

	// Step by step verify labels in a name with multiple dots.
	std::size_t last_dot = dots.pos[0];
	std::size_t next_dot = dots.pos[1];
	// The domain suffix verified so far.
	auto verified = last_dot;
	bool wildcard = lookup_first(name.substr(verified + 1));
	// The next label to verify.
	auto label = name.substr(next_dot + 1);
	Node *node = lookup_second_level(label);
	while (node) {
		if (node->rule == Rule::kException)
			verified = last_dot;
		else if (node->rule == Rule::kRegular || wildcard)
			verified = next_dot;
		wildcard = node->wildcard;
		last_dot = next_dot;

		next_dot = name.find_last_of('.', last_dot - 1);
		if (next_dot == std::string::npos) {
			label = name.substr(0, last_dot);
			Node *next = lookup_next_level(node, label);
			if (next && next->rule == Rule::kException)
				return name.substr(last_dot + 1);
			if (!wildcard && (!next || next->rule == Rule::kDefault))
				return name.substr(verified + 1);
			return name;
		}

		label = name.substr(next_dot + 1, last_dot - next_dot - 1);
		node = lookup_next_level(node, label);
	}

	if (wildcard)
		verified = next_dot;
	return name.substr(verified + 1);
}

} // namespace public_suffix

#endif // PUBLIC_SUFFIX_LOOKUP_H
