/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef PADDING_HPP
#define PADDING_HPP

#include <cstddef>

#define CACHELINE_SIZE 128


template<class T, size_t Size = CACHELINE_SIZE>

class Padded : public T {

	using T::T;

	constexpr static size_t roundup(size_t const x, size_t const y)
	{
		return (((x + (y - 1)) / y) * y);
	}

	uint8_t padding[roundup(sizeof(T), Size)-sizeof(T)];

public:

	inline T *ptr_to_basetype()
	{
		return (T *) this;
	}
};

#endif // PADDING_HPP
