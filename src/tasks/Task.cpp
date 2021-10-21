/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nosv.h>

#include <nanos6/final.h>

#include "tasks/TaskMetadata.hpp"


extern "C" signed int nanos6_in_final(void)
{
	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(currentTask);
	assert(taskMetadata != nullptr);

	return taskMetadata->isFinal();
}

extern "C" signed int nanos6_in_serial_context(void)
{
	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(currentTask);
	assert(taskMetadata != nullptr);

	return taskMetadata->isFinal() || taskMetadata->isIf0();
}
