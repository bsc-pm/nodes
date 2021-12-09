/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>

#include <nosv.h>

#include <nanos6/task-instantiation.h>

#include "common/ErrorHandler.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "dependencies/discrete/TaskDataAccesses.hpp"
#include "dependencies/discrete/TaskDataAccessesInfo.hpp"
#include "memory/MemoryAllocator.hpp"
#include "hardware/HardwareInfo.hpp"
#include "tasks/TaskloopMetadata.hpp"
#include "tasks/TaskMetadata.hpp"


void nanos6_create_task(
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *,
	size_t argsBlockSize,
	void **argsBlockPointer,
	void **taskPointer,
	size_t flags,
	size_t numDeps
) {
	size_t originalArgsBlockSize = argsBlockSize;

	// Get the necessary size to create the task
	size_t taskSize = 0;
	bool isTaskloop = (flags & nanos6_taskloop_task);
	if (isTaskloop) {
		taskSize += sizeof(TaskloopMetadata);
	} else {
		taskSize += sizeof(TaskMetadata);
	}

	// Get the necessary size for the task's dependencies
	TaskDataAccessesInfo taskAccesses(numDeps);
	size_t taskAccessesSize = taskAccesses.getAllocationSize();

	bool hasPreallocatedArgsBlock = (flags & nanos6_preallocated_args_block);
	if (hasPreallocatedArgsBlock) {
		assert(argsBlockPointer != nullptr);

		taskSize += taskAccessesSize;
	} else {
		// Alignment fixup
		size_t missalignment = argsBlockSize & (DATA_ALIGNMENT_SIZE - 1);
		size_t correction = (DATA_ALIGNMENT_SIZE - missalignment) & (DATA_ALIGNMENT_SIZE - 1);
		argsBlockSize += correction;

		// Allocation and layout
		taskSize += argsBlockSize + taskAccessesSize;
	}

	// nOS-V Might not be able to allocate the argsBlocks
	bool locallyAllocated = (taskSize > NOSV_MAX_METADATA_SIZE);

	// Create the nOS-V task
	nosv_task_type_t tasktype = (nosv_task_type_t) taskInfo->task_type_data;
	assert(tasktype != nullptr);

	nosv_task_t task;
	int ret = nosv_create(
		&task,
		tasktype,
		(locallyAllocated) ? sizeof(void *) : sizeof (void *) + taskSize,
		NOSV_CREATE_NONE
	);
	assert(!ret);

	// Since we won't know if the metadata returned by nOS-V is a memory region
	// or a pointer that points to a locally allocated region, we always alloc
	// extra space for a pointer that references the metadata's real region
	// - If nOS-V can allocate the memory, the pointer will point to the memory
	// allocated right next to it
	// - If nOS-V can't allocate the memory, the pointer will point to a region
	// of memory allocated by NODES
	void **metadataPointer = (void **) nosv_get_task_metadata(task);
	assert(metadataPointer != nullptr);

	if (locallyAllocated) {
		*metadataPointer = MemoryAllocator::alloc(taskSize);
	} else {
		*metadataPointer = ((char *) metadataPointer + sizeof(void *));
	}

	void *metadata = *metadataPointer;
	assert(metadata != nullptr);

	if (!hasPreallocatedArgsBlock) {
		if (isTaskloop) {
			// Skip sizeof(TaskloopMetadata) to assign the pointer
			*argsBlockPointer = (void *) ((char *) metadata + sizeof(TaskloopMetadata));
		} else {
			// Skip sizeof(TaskMetadata) to assign the pointer
			*argsBlockPointer = (void *) ((char *) metadata + sizeof(TaskMetadata));
		}
	}

	// Compute the correct address for the task's accesses
	taskAccesses.setAllocationAddress((char *) *argsBlockPointer + argsBlockSize);

	// Retreive and construct the task's metadata
	if (isTaskloop) {
		new (metadata) TaskloopMetadata(*argsBlockPointer, originalArgsBlockSize, task, flags, taskAccesses, taskSize, locallyAllocated);
	} else {
		new (metadata) TaskMetadata(*argsBlockPointer, originalArgsBlockSize, task, flags, taskAccesses, taskSize, locallyAllocated);
	}

	// Assign the nOS-V task pointer for a future submit
	*taskPointer = (void *) task;
}

void nanos6_create_loop(
	nanos6_task_info_t *task_info,
	nanos6_task_invocation_info_t *task_invocation_info,
	size_t args_block_size,
	/* OUT */ void **args_block_pointer,
	/* OUT */ void **task_pointer,
	size_t flags,
	size_t num_deps,
	size_t lower_bound,
	size_t upper_bound,
	size_t grainsize,
	size_t chunksize
) {
	assert(task_info->implementation_count == 1);

	// The compiler passes either the num deps of a single child or -1. However, the parent taskloop
	// must register as many deps as num_deps * numTasks
	bool isTaskloop = flags & nanos6_taskloop_task;
	if (num_deps != (size_t) -1 && isTaskloop) {
		size_t numTasks = Taskloop::computeNumTasks((upper_bound - lower_bound), grainsize);
		num_deps *= numTasks;
	}

	nanos6_create_task(
		task_info, task_invocation_info, args_block_size,
		args_block_pointer, task_pointer, flags, num_deps
	);

	TaskloopMetadata *taskloopMetadata = (TaskloopMetadata *) TaskMetadata::getTaskMetadata((nosv_task_t) (*task_pointer));
	assert(*task_pointer != nullptr);
	assert(taskloopMetadata != nullptr);
	assert(taskloopMetadata->isTaskloop());

	taskloopMetadata->initialize(lower_bound, upper_bound, grainsize, chunksize);
}

//! Public API function to submit tasks
void nanos6_submit_task(void *taskHandle)
{
	nosv_task_t task = (nosv_task_t) taskHandle;
	assert(task != nullptr);

	nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(task);
	assert(taskInfo != nullptr);

	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);

	// Obtain the parent task and link both parent and child
	nosv_task_t parentTask = nosv_self();
	if (parentTask != nullptr) {
		taskMetadata->setParent(parentTask);
	}

	// Register the accesses of the task to check whether it is ready to be executed
	bool ready = true;
	if (taskInfo->register_depinfo != nullptr) {
		int cpuId = nosv_get_current_logical_cpu();
		CPUDependencyData *cpuDepData = HardwareInfo::getCPUDependencyData(cpuId);
		ready = DataAccessRegistration::registerTaskDataAccesses(taskMetadata, *cpuDepData);
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
			taskMetadata->markIf0AsNotInlined();
			nosv_pause(NOSV_SUBMIT_NONE);
		}
	}
}
