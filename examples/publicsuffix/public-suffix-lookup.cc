#include <cstdlib>
#include <iostream>

#include "public-suffix-types.h"

// This header is generated.
#include "public-suffix-tables.h"

using namespace phf;
using namespace public_suffix;

int
main(int ac, char *av[])
{
	for (--ac, ++av; ac; --ac, ++av) {
		std::cout << av[0] << '\n';
	}

	return EXIT_SUCCESS;
}
