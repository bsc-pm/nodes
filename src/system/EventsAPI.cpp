/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstddef>

#include <nosv.h>

#include <nodes/events.h>

#include "tasks/TaskMetadata.hpp"

extern "C" void *nanos6_get_current_event_counter(void)
{
	nosv_task_t current_task = nosv_self();
	assert(current_task != nullptr);

	return (void *) current_task;
}

extern "C" void nanos6_increase_current_task_event_counter(void *, unsigned int increment)
{
	nosv_task_t current_task = nosv_self();
	assert(current_task != nullptr);
	TaskMetadata *metadata = TaskMetadata::getTaskMetadata(current_task);
	assert(metadata != nullptr);
	metadata->markAsCommunicationTask();

	nosv_increase_event_counter(increment);
}

extern "C" void nanos6_decrease_task_event_counter(void *event_counter, unsigned int decrement)
{
	assert(event_counter != nullptr);

	nosv_decrease_event_counter((nosv_task_t) event_counter, decrement);
}
