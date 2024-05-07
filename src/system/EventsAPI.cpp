/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstddef>

#include <nosv.h>

#include <nodes/events.h>

#include "common/ErrorHandler.hpp"
#include "tasks/TaskMetadata.hpp"

extern "C" void *nanos6_get_current_event_counter(void)
{
	nosv_task_t current_task = nosv_self();
	assert(current_task != nullptr);

	return (void *) current_task;
}

extern "C" void nanos6_increase_current_task_event_counter(void *, unsigned int increment)
{
	TaskMetadata *metadata = TaskMetadata::getCurrentTask();
	assert(metadata != nullptr);

	// Mark this task as a potential communication task (TAMPI), which we need for some
	// optimizations related with the taskiter construct
	metadata->markAsCommunicationTask();

	if (int err = nosv_increase_event_counter(increment))
		ErrorHandler::fail("nosv_increase_event_counter failed: ", nosv_get_error_string(err));
}

extern "C" void nanos6_decrease_task_event_counter(void *event_counter, unsigned int decrement)
{
	assert(event_counter != nullptr);

	if (int err = nosv_decrease_event_counter((nosv_task_t) event_counter, decrement))
		ErrorHandler::fail("nosv_decrease_event_counter failed: ", nosv_get_error_string(err));
}
