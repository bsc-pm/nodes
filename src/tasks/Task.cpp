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
	TaskMetadata *taskMetadata = TaskMetadata::getCurrentTask();
	assert(taskMetadata != nullptr);
	return taskMetadata->isFinal();
}

extern "C" signed int nanos6_in_serial_context(void)
{
	TaskMetadata *taskMetadata = TaskMetadata::getCurrentTask();
	assert(taskMetadata != nullptr);
	return taskMetadata->isFinal() || taskMetadata->isIf0();
}
