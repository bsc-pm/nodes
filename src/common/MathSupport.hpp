/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef MATH_SUPPORT_HPP
#define MATH_SUPPORT_HPP

#include <cstdint>


namespace MathSupport {

	static inline size_t ceil(size_t x, size_t y)
	{
		return (x + (y - 1)) / y;
	}

	constexpr uint64_t roundup(const uint64_t x, const uint64_t y)
	{
		return (((x + (y - 1ULL)) / y) * y);
	}

	inline uint64_t roundToNextPowOf2(const uint64_t x)
	{
		return roundup(x, 1ULL << (63 - __builtin_clzll(x)));
	}

	inline bool isPowOf2(const uint64_t x)
	{
		return (__builtin_popcountll(x) == 1);
	}
}

#endif // MATH_SUPPORT_HPP
