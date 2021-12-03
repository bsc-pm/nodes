/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef MEMORY_ALLOCATOR_HPP
#define MEMORY_ALLOCATOR_HPP

#include <cstdlib>
#include <malloc.h>
#include <memory>

#include "common/ErrorHandler.hpp"


class MemoryAllocator {

public:

	static inline void *alloc(size_t size)
	{
		void *ptr = malloc(size);
		ErrorHandler::failIf(ptr == nullptr, " when trying to allocate memory");
		return ptr;
	}

	static inline void free(void *chunk, __attribute__((unused)) size_t size)
	{
		std::free(chunk);
	}

	/* Simplifications for using "new" and "delete" with the allocator */
	template <typename T, typename... Args>
	static T *newObject(Args &&... args)
	{
		void *ptr = MemoryAllocator::alloc(sizeof(T));
		new (ptr) T(std::forward<Args>(args)...);
		return (T*)ptr;
	}

	template <typename T>
	static void deleteObject(T *ptr)
	{
		ptr->~T();
		MemoryAllocator::free(ptr, sizeof(T));
	}
};

template<typename T>
using TemplateAllocator = std::allocator<T>;

#endif // MEMORY_ALLOCATOR_HPP
