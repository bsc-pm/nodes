/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef OBJECT_ALLOCATOR_HPP
#define OBJECT_ALLOCATOR_HPP

#include "MemoryAllocator.hpp"


template <typename T>
class ObjectAllocator {

public:

	template <typename... ARGS>
	static inline T *newObject(ARGS &&... args)
	{
		return MemoryAllocator::newObject<T>(std::forward<ARGS>(args)...);
	}

	static inline void deleteObject(T *ptr)
	{
		MemoryAllocator::deleteObject<T>(ptr);
	}
};

#endif // OBJECT_ALLOCATOR_HPP
