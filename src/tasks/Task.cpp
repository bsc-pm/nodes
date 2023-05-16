/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nosv.h>

#include <nodes/final.h>

#include "tasks/TaskMetadata.hpp"


extern "C" signed int nanos6_in_final(void)
{
	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(currentTask);
	return taskMetadata->isFinal();
}

extern "C" signed int nanos6_in_serial_context(void)
{
	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(currentTask);
	return taskMetadata->isFinal() || taskMetadata->isIf0();
}
