#ifndef PUBLIC_SUFFIX_LOOKUP_H
#define PUBLIC_SUFFIX_LOOKUP_H

#include "public-suffix-types.h"

// This header is generated.
#include "public-suffix-tables.h"

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
	// Check if the name contains at least one dot.
	auto last_dot = name.find_last_of('.');
	if (last_dot == std::string::npos)
		return name;

	// Check if the name contains exactly one dot.
	auto next_dot = name.find_last_of('.', last_dot - 1);
	if (next_dot == std::string::npos) {
		Node *node = lookup_second_level(name);
		if (node && node->rule == Rule::kException)
			return name.substr(last_dot + 1);
		if (!node || node->rule == Rule::kDefault) {
			auto label = name.substr(last_dot + 1);
			if (!lookup_first(label))
				return label;
		}
		return name;
	}

	// The name part matched so far.
	auto verified = last_dot;
	bool wildcard = false;

	// Step by step verify labels in the name with multiple dots.
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
