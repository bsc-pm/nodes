/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>

#include <api/task-instantiation.h>

#include "TaskFinalization.hpp"
#include "system/SpawnFunction.hpp"
#include "system/TaskCreation.hpp"


void TaskFinalization::taskFinished(nosv_task_t task)
{
	assert(task != nullptr);

	nosv_task_type_t type = nosv_get_task_type(task);
	assert(type != nullptr);

	nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
	assert(taskInfo != nullptr);

	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Call the taskinfo destructor if not null
	void *argsBlocks = taskMetadata->_argsBlock;
	if (taskInfo->destroy_args_block != nullptr) {
		taskInfo->destroy_args_block(argsBlocks);
	}

	if (taskMetadata->_isSpawned) {
		SpawnFunction::_pendingSpawnedFunctions--;
	}

	nosv_destroy(task, NOSV_DESTROY_NONE);
}
