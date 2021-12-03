/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <nanos6/taskwait.h>

#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "hardware/HardwareInfo.hpp"
#include "tasks/TaskMetadata.hpp"


//! \brief Block the control flow of the current task until all of its children have finished
//!
//! \param[in] invocationSource A string that identifies the source code location of the invocation
extern "C" void nanos6_taskwait(char const */*invocationSource*/)
{
	// Retreive the task's metadata
	TaskMetadata *taskMetadata = TaskMetadata::getCurrentTask();
	if (taskMetadata->doesNotNeedToBlockForChildren()) {
		std::atomic_thread_fence(std::memory_order_acquire);
		return;
	}

	DataAccessRegistration::handleEnterTaskwait(taskMetadata);
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

	DataAccessRegistration::handleExitTaskwait(taskMetadata);
}
