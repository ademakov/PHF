#include <cstdlib>
#include <iostream>

#include "public-suffix-types.h"

// This header is generated.
#include "public-suffix-tables.h"

using namespace phf;
using namespace public_suffix;

Node*
lookup_first_level(const std::string &label)
{
	auto begin = &first_level_nodes[0];
	auto end = &first_level_nodes[0] + sizeof(first_level_nodes) / sizeof(first_level_nodes[0]);
	auto it = std::find_if(begin, end, [label](Node& x) { return label == x.label; });
	if (it == end)
		return nullptr;
	return it;
}

Node*
lookup_second_level(const std::string &label)
{
	auto rank = second_level_index::instance[label];
	if (rank == phf::not_found)
		return nullptr;

	Node* node = &second_level_nodes[rank];
	if (label != node->label)
		return nullptr;
	if (node->rule == Rule::kDefault || node->rule == Rule::kWildcard)
		return nullptr;
	return node;
}

std::string
lookup(const std::string &name)
{
	auto dot_1 = name.find_last_of('.');
	if (dot_1 == std::string::npos)
		return name;

	auto dot_2 = name.find_last_of('.', dot_1 - 1);
	if (dot_2 == std::string::npos) {
		if (!lookup_second_level(name)) {
			auto label = name.substr(dot_1 + 1);
			if (!lookup_first_level(label))
				return label;
		}
		return name;
	}

	auto rank = second_level_index::instance[name.substr(dot_2 + 1)];
	if (rank != phf::not_found)
		return std::to_string(rank);
	return "xx";
}

int
main(int ac, char *av[])
{
	if (ac < 2) {
		std::cerr << "Usage: " << av[0] << " <domain-name>...\n";
		return EXIT_FAILURE;
	}

	for (--ac, ++av; ac; --ac, ++av) {
		std::string suffix = lookup(av[0]);
		std::cout << av[0] << ' ' << suffix << '\n';
	}

	return EXIT_SUCCESS;
}
