/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>

#include <nosv.h>

#include <api/task-instantiation.h>

#include "TaskCreation.hpp"


void nanos6_create_task(
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *taskInvocationInfo,
	size_t argsBlockSize,
	void **argsBlockPointer,
	void **taskPointer,
	size_t flags,
	size_t numDeps
) {
	// Get the necessary size to create the task
	size_t taskSize = 0;
	bool hasPreallocatedArgsBlock = (flags & nanos6_preallocated_args_block);
	if (hasPreallocatedArgsBlock) {
		assert(argsBlockPointer != nullptr);

		taskSize += sizeof(TaskMetadata);
	} else {
		// Alignment fixup
		size_t missalignment = argsBlockSize & (DATA_ALIGNMENT_SIZE - 1);
		size_t correction = (DATA_ALIGNMENT_SIZE - missalignment) & (DATA_ALIGNMENT_SIZE - 1);

		// Allocation and layout
		taskSize += argsBlockSize + correction + sizeof(TaskMetadata);
	}

	// NOTE: For now error, later on fix
	ErrorHandler::failIf(taskSize > NOSV_MAX_METADATA_SIZE, "Task argsBlock size too large (max 4K)");

	nosv_task_type_t tasktype = (nosv_task_type_t) taskInfo->task_type_data;
	assert(tasktype != nullptr);

	// Create the nOS-V task
	nosv_task_t task;
	int ret = nosv_create(&task, tasktype, taskSize, NOSV_CREATE_NONE);
	assert(!ret);

	// Retreive the task's metadata
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// If the task doesn't have preallocated args block, set everything up
	if (!hasPreallocatedArgsBlock) {
		// Skip sizeof(TaskMetadata) to assign the pointer
		taskMetadata->_argsBlock = (void *) ((char *) taskMetadata + sizeof(TaskMetadata));

		// Assign the nOS-V allocated metadata as the argsBlocks
		*argsBlockPointer = taskMetadata->_argsBlock;
	}

	// For now, set the task as non-spawned
	taskMetadata->_isSpawned = false;

	// Assign the nOS-V task pointer for a future submit
	*taskPointer = (void *) task;
}

//! Public API function to submit tasks
void nanos6_submit_task(void *taskHandle)
{
	// TODO: Compute dependencies, decide if ready or not

	// Submit the task to nOS-V
	nosv_submit((nosv_task_t) taskHandle, NOSV_SUBMIT_NONE);
}
