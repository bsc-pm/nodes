/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <api/taskwait.h>

#include "TaskCreation.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "hardware/HardwareInfo.hpp"


//! \brief Block the control flow of the current task until all of its children have finished
//!
//! \param[in] invocationSource A string that identifies the source code location of the invocation
extern "C" void nanos6_taskwait(char const */*invocationSource*/)
{
	nosv_task_t task = nosv_self();

	// Retreive the task's metadata
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	if (taskMetadata->doesNotNeedToBlockForChildren()) {
		std::atomic_thread_fence(std::memory_order_acquire);
		return;
	}

	DataAccessRegistration::handleEnterTaskwait(task);
	bool done = taskMetadata->markAsBlocked();

	// done == true:
	//   1. The condition of the taskwait has been fulfilled
	//   2. The task will not be queued at all
	//   3. The execution must continue (without blocking)
	// done == false:
	//   1. The task has been marked as blocked
	//   2. At any time the condition of the taskwait can become true
	//   3. The task responsible for that change will re-queue the parent
	if (!done) {
		nosv_pause(NOSV_SUBMIT_NONE);
	}

	std::atomic_thread_fence(std::memory_order_acquire);

	assert(taskMetadata->canBeWokenUp());
	taskMetadata->markAsUnblocked();

	DataAccessRegistration::handleExitTaskwait(task);
}
