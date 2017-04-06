#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <getopt.h>

#include "public-suffix-lookup.h"

using namespace public_suffix;

option options[] = {{"input", required_argument, nullptr, 'i'},
		    {"output", required_argument, nullptr, 'o'},
		    {nullptr, 0, nullptr, 0}};

const char *prog_name = nullptr;

[[noreturn]] void
usage()
{
	std::fprintf(stderr,
		     "Usage: %s [-i <input-file>] [-o <output-file>] [<domain-name>...]\n",
		     prog_name);
	std::exit(EXIT_FAILURE);
}

int
main(int ac, char *av[])
{
	const char *in_name = nullptr;
	const char *out_name = nullptr;

	int c;
	prog_name = av[0];
	while ((c = getopt_long(ac, av, "i:o:", options, NULL)) != -1) {
		switch (c) {
		case 'i':
			in_name = optarg;
			break;
		case 'o':
			out_name = optarg;
			break;
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	FILE *out;
	if (!out_name) {
		out = stdout;
	} else {
		out = std::fopen(out_name, "w");
		if (!out) {
			std::fprintf(stderr, "Failed to open file: %s\n", out_name);
			std::exit(EXIT_FAILURE);
		}
	}

	if (in_name) {
		FILE *in = std::fopen(in_name, "r");
		if (!in) {
			if (out_name)
				std::fclose(out);
			std::fprintf(stderr, "Failed to open file: %s\n", in_name);
			std::exit(EXIT_FAILURE);
		}

		char buffer[1024];
		while (std::fgets(buffer, 1024, in)) {
			auto n = std::strlen(buffer);
			if (buffer[n - 1] == '\n')
				buffer[n - 1] = 0;

			auto suffix = lookup(buffer);
			std::fputs(suffix.data(), out);
			std::fputc('\n', out);
		}

		std::fclose(in);
	}

	for (; ac; --ac, ++av) {
		auto suffix = lookup(av[0]);
		std::fputs(suffix.data(), out);
		std::fputc('\n', out);
	}

	if (out_name)
		std::fclose(out);
	return EXIT_SUCCESS;
}
