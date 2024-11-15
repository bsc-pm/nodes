/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2024 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nodes/coroutines.hpp>

#include "tasks/TaskMetadata.hpp"

void *oss_coroutine::promise_type::operator new(size_t size)
{
	nosv_task_t task = nosv_self();
	if (task != nullptr) {
		TaskMetadata *metadata = TaskMetadata::getTaskMetadata(task);
		assert(metadata != nullptr);

		void *coroFrame = metadata->getCoroFrameAddr(size);
		if (coroFrame != nullptr) {
			return coroFrame;
		}
	}

	return malloc(size);
}

void oss_coroutine::promise_type::operator delete(void *ptr)
{
	nosv_task_t task = nosv_self();
	if (task == nullptr) {
		assert(ptr != nullptr);

		free(ptr);
	} else {
		TaskMetadata *metadata = TaskMetadata::getTaskMetadata(task);
		assert(metadata != nullptr);

		if (metadata->hasCoroFrame()) {
			metadata->freeCoroFrame();
		} else {
			assert(ptr != nullptr);

			free(ptr);
		}
	}

	ptr = nullptr;
}
