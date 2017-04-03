#include <cstdlib>
#include <iostream>

#include "public-suffix-lookup.h"

using namespace public_suffix;

int
main(int ac, char *av[])
{
	if (ac < 2) {
		std::cerr << "Usage: " << av[0] << " <domain-name>...\n";
		return EXIT_FAILURE;
	}

	for (--ac, ++av; ac; --ac, ++av) {
		auto suffix = lookup(av[0]);
		std::cout << av[0] << ' ' << suffix << '\n';
	}

	return EXIT_SUCCESS;
}
