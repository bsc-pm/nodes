/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>
#include <sched.h>

#include <nosv.h>

#include <api/task-instantiation.h>

#include "TaskCreation.hpp"
#include "common/ErrorHandler.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "dependencies/discrete/TaskDataAccesses.hpp"
#include "dependencies/discrete/TaskDataAccessesInfo.hpp"
#include "hardware/HardwareInfo.hpp"


void nanos6_create_task(
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *taskInvocationInfo,
	size_t argsBlockSize,
	void **argsBlockPointer,
	void **taskPointer,
	size_t flags,
	size_t numDeps
) {
	// Get the necessary size for the task's dependencies
	TaskDataAccessesInfo taskAccesses(numDeps);
	size_t taskAccessesSize = taskAccesses.getAllocationSize();

	// Get the necessary size to create the task
	size_t taskSize = 0;
	bool hasPreallocatedArgsBlock = (flags & nanos6_preallocated_args_block);
	if (hasPreallocatedArgsBlock) {
		assert(argsBlockPointer != nullptr);

		// Allocation
		taskSize += taskAccessesSize + sizeof(TaskMetadata);
	} else {
		// Alignment fixup
		size_t missalignment = argsBlockSize & (DATA_ALIGNMENT_SIZE - 1);
		size_t correction = (DATA_ALIGNMENT_SIZE - missalignment) & (DATA_ALIGNMENT_SIZE - 1);
		argsBlockSize += correction;

		// Allocation and layout
		taskSize += argsBlockSize + taskAccessesSize + sizeof(TaskMetadata);
	}
	// NOTE: If the size exceeds, error for now
	ErrorHandler::failIf(taskSize > NOSV_MAX_METADATA_SIZE, "Task argsBlock size too large (max 4K)");


	// Create the nOS-V task
	nosv_task_type_t tasktype = (nosv_task_type_t) taskInfo->task_type_data;
	assert(tasktype != nullptr);

	nosv_task_t task;
	int ret = nosv_create(&task, tasktype, taskSize, NOSV_CREATE_NONE);
	assert(!ret);

	// Retreive the task's metadata
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Set the task's args block pointer if it doesn't preallocate them
	if (!hasPreallocatedArgsBlock) {
		// Skip sizeof(TaskMetadata) to assign the pointer
		taskMetadata->_argsBlock = (void *) ((char *) taskMetadata + sizeof(TaskMetadata));

		// Assign the nOS-V allocated metadata as the argsBlocks
		*argsBlockPointer = taskMetadata->_argsBlock;
	}

	// Set the allocation address of the task accesses and copy internals
	taskAccesses.setAllocationAddress((char *) taskMetadata + sizeof(TaskMetadata) + argsBlockSize);
	new (&(taskMetadata->_dataAccesses)) TaskDataAccesses(taskAccesses);

	// Initialize the attributes of the task
	taskMetadata->_predecessorCount = 0;
	taskMetadata->_removalCount = 1;
	taskMetadata->_countdownToBeWokenUp = 1;
	taskMetadata->_countdownToRelease = 1;
	taskMetadata->_parent = nullptr;
	taskMetadata->_finished = false;
	taskMetadata->_flags = flags;

	// Assign the nOS-V task pointer for a future submit
	*taskPointer = (void *) task;
}

//! Public API function to submit tasks
void nanos6_submit_task(void *taskHandle)
{
	nosv_task_t task = (nosv_task_t) taskHandle;
	assert(task != nullptr);

	nosv_task_type_t type = nosv_get_task_type(task);
	assert(type != nullptr);

	nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
	assert(taskInfo != nullptr);

	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Obtain the parent task and link both parent and child
	nosv_task_t parentTask = nosv_self();
	if (parentTask != nullptr) {
		taskMetadata->setParent(parentTask);
	}

	// Register the accesses of the task to check whether it is ready to be executed
	bool ready = true;
	if (taskInfo->register_depinfo != nullptr) {
		int cpuId = sched_getcpu();
		CPUDependencyData *cpuDepData = HardwareInfo::getCPUDependencyData(cpuId);
		ready = DataAccessRegistration::registerTaskDataAccesses(task, *cpuDepData);
	}

	bool isIf0 = taskMetadata->isIf0();
	assert(parentTask != nullptr || ready);
	assert(parentTask != nullptr || !isIf0);

	if (ready && !isIf0) {
		// Submit the task to nOS-V if ready and not if0
		nosv_submit(task, NOSV_SUBMIT_UNLOCKED);
	}

	// Special handling for if0 tasks
	if (isIf0) {
		if (ready) {
			// Ready if0 tasks are executed inline
			nosv_submit(task, NOSV_SUBMIT_INLINE);
		} else {
			// Non-ready if0 tasks cause this task to get paused. Before the
			// if0 task starts executing (after deps are satisfied), it will
			// detect that it was a non-ready if0 task and re-submit the parent
			nosv_pause(NOSV_SUBMIT_NONE);
		}
	}
}
