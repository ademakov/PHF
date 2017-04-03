//
// The code in this file is free and unencumbered software released
// into the public domain.
//
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
#ifndef PERFECT_HASH_RNG_H
#define PERFECT_HASH_RNG_H

#include <cstdint>
#include <random>
#include <thread>

#include <x86intrin.h>

namespace rng {

//
// Bitwise circular left shift.
//
static inline std::uint64_t
rotl(const std::uint64_t x, int k)
{
	return (x << k) | (x >> (64 - k));
}

//
// A simple random seed generator based on the entropy coming from the
// system thread/process scheduler. This is rather slow but seeds are
// normally generated very infrequently.
//
struct tsc_seed
{
	using result_type = std::uint64_t;

	result_type operator()()
	{
		std::uint64_t base = _rdtsc();
		std::uint64_t seed = base & 0xff;
		for (int i = 1; i < 8; i++) {
			std::this_thread::yield();
			seed |= ((_rdtsc() - base) & 0xff) << (i << 3);
		}
		return seed;
	}
};

//
// A random seed generator based on std::random_device.
//
struct random_device_seed
{
	using result_type = std::uint64_t;

	result_type operator()()
	{
		std::random_device rd;
		if (sizeof(result_type) > sizeof(std::random_device::result_type))
			return rd() | (result_type{rd()} << 32);
		else
			return rd();
	}
};

//
// A random number generator with 64-bit internal state. It is based on
// the code from here: http://xoroshiro.di.unimi.it/splitmix64.c
//
struct rng64
{
	using result_type = std::uint64_t;

	std::uint64_t state;

	rng64(std::uint64_t seed = 1) : state{seed}
	{
	}

	result_type operator()()
	{
		std::uint64_t z = (state += UINT64_C(0x9E3779B97F4A7C15));
		z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
		z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
		return z ^ (z >> 31);
	}
};

//
// A random number generator with 128-bit internal state. It is based on
// the code from here: http://xoroshiro.di.unimi.it/xoroshiro128plus.c
//
struct rng128
{
	using result_type = std::uint64_t;

	std::uint64_t state[2];

	rng128(std::uint64_t seed[2]) : state{seed[0], seed[1]}
	{
	}

	rng128(std::uint64_t s0, std::uint64_t s1) : state{s0, s1}
	{
	}

	rng128(std::uint64_t seed = 1)
	{
		rng64 seeder(seed);
		state[0] = seeder();
		state[1] = seeder();
	}

	result_type operator()()
	{
		const uint64_t s0 = state[0];
		uint64_t s1 = state[1];
		const uint64_t value = s0 + s1;

		s1 ^= s0;
		state[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
		state[1] = rotl(s1, 36);

		return value;
	}

	// This is the jump function for the generator. It is equivalent
	// to 2 ^ 64 calls to next(); it can be used to generate 2 ^ 64
	// non-overlapping subsequences for parallel computations.
	void jump()
	{
		static const std::uint64_t j[] = {0xbeac0467eba5facb, 0xd86b048b86aa9922};

		std::uint64_t s0 = 0, s1 = 0;
		for (std::size_t i = 0; i < sizeof j / sizeof j[0]; i++) {
			for (int b = 0; b < 64; b++) {
				if ((j[i] & UINT64_C(1) << b) != 0) {
					s0 ^= state[0];
					s1 ^= state[1];
				}
				operator()();
			}
		}

		state[0] = s0;
		state[1] = s1;
	}
};

} // namespace rng

#endif // PERFECT_HASH_RNG_H
