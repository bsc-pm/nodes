/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>

#include <nosv.h>

#include <nodes/task-instantiation.h>

#include "TaskFinalization.hpp"
#include "common/ErrorHandler.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "dependencies/discrete/taskiter/TaskGroupMetadata.hpp"
#include "hardware/HardwareInfo.hpp"
#include "memory/MemoryAllocator.hpp"
#include "system/SpawnFunction.hpp"
#include "tasks/TaskMetadata.hpp"


void TaskFinalization::taskEndedCallback(nosv_task_t task)
{
	assert(task);

	nosv_task_t lastTask = TaskMetadata::getLastTask();
	TaskMetadata::setLastTask(task);

	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);

	// Negative returned values mean the call failed, and the error is codified within
	int cpuId = nosv_get_current_logical_cpu();
	if (cpuId < 0) {
		ErrorHandler::fail("nosv_get_current_logical_cpu failed: ", nosv_get_error_string(cpuId));
	}

	DataAccessRegistration::combineTaskReductions(taskMetadata, cpuId);

	TaskMetadata::setLastTask(lastTask);
}

void TaskFinalization::taskCompletedCallback(nosv_task_t task)
{
	assert(task);

	nosv_task_t lastTask = TaskMetadata::getLastTask();
	TaskMetadata::setLastTask(task);

	// Mark that the task has finished user code execution
	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
	taskMetadata->markAsFinished();

	// If the task has a wait clause, the release of dependencies must be
	// delayed (at least) until the task finishes its execution and all
	// its children complete and become disposable
	bool dependenciesReleaseable = true;
	if (taskMetadata->mustDelayRelease()) {
		DataAccessRegistration::handleEnterTaskwait(taskMetadata);
		if (!taskMetadata->markAsBlocked()) {
			dependenciesReleaseable = false;
		} else {
			// All its children are completed, so the delayed release as well
			taskMetadata->completeDelayedRelease();
			DataAccessRegistration::handleExitTaskwait(taskMetadata);
			taskMetadata->markAsUnblocked();
		}
	}

	if (dependenciesReleaseable) {
		dependenciesReleaseable = taskMetadata->decreaseReleaseCount();
	}

	// Check whether all external events have been also fulfilled, so the dependencies can be released
	if (dependenciesReleaseable) {
		// Check whether this is an external thread (no cpuDepData)
		bool isExternal = (nosv_self() == nullptr);
		CPUDependencyData *cpuDepData;
		if (isExternal) {
			cpuDepData = new CPUDependencyData();
		} else {
			int cpuId = nosv_get_current_logical_cpu();
			if (cpuId < 0) {
				ErrorHandler::fail("nosv_get_current_logical_cpu failed: ", nosv_get_error_string(cpuId));
			}
			cpuDepData = HardwareInfo::getCPUDependencyData(cpuId);
		}

		bool finish = DataAccessRegistration::unregisterTaskDataAccesses(taskMetadata, *cpuDepData, lastTask != nullptr);
		// Here taskiter tasks may already be reenqueued

		if (isExternal) {
			delete cpuDepData;
		}

		if (finish) {
			if (taskMetadata->isGroup())
				((TaskGroupMetadata *) taskMetadata)->finalizeGroupedTasks();

			TaskFinalization::taskFinished(taskMetadata);
		}

		if (taskMetadata->decreaseRemovalBlockingCount()) {
			assert(finish);
			TaskFinalization::disposeTask(taskMetadata);
		}
	}

	TaskMetadata::setLastTask(lastTask);
}

void TaskFinalization::taskFinished(TaskMetadata *task)
{
	// Decrease the _countdownToBeWokenUp of the task, which was initialized to 1
	// If it becomes 0, we can propagate the counter through its parents
	TaskMetadata *taskMetadata = task;
	bool ready = taskMetadata->finishChild();

	// NOTE: Needed?
	// We always use a local CPUDependencyData struct here to avoid issues
	// with re-using an already used CPUDependencyData
	CPUDependencyData *localHpDependencyData = nullptr;
	TaskMetadata *parentMetadata = nullptr;
	while ((taskMetadata != nullptr) && ready) {
		parentMetadata = taskMetadata->getParent();

		// If this is the first iteration of the loop, the task will test true
		// to hasFinished and false to mustDelayRelease, which does nothing
		if (taskMetadata->hasFinished()) {
			if (taskMetadata->mustDelayRelease()) {
				taskMetadata->completeDelayedRelease();
				DataAccessRegistration::handleExitTaskwait(taskMetadata);
				taskMetadata->markAsUnblocked();

				// Check whether all external events have been also fulfilled
				bool dependenciesReleaseable = taskMetadata->decreaseReleaseCount();
				if (dependenciesReleaseable) {
					if (localHpDependencyData == nullptr) {
						localHpDependencyData = new CPUDependencyData();
					}

					bool finish = DataAccessRegistration::unregisterTaskDataAccesses(taskMetadata, *localHpDependencyData, true);

					// This is just to emulate a recursive call to TaskFinalization::taskFinished() again
					// It should not return false because at this point delayed release has happenned which means that
					// the task has gone through a taskwait (no more children should be unfinished)

					if (finish) {
						ready = taskMetadata->finishChild();
						assert(ready);
					} else {
						ready = false;
					}

					if (taskMetadata->decreaseRemovalBlockingCount()) {
						assert(finish);
						TaskFinalization::disposeTask(taskMetadata);
					}
				} else if (taskMetadata->isTaskiter()) {
					// This can only happen the first time a taskiter is released,
					// which means that we have to stop the recursive process
					break;
				}
			}
		} else {
			// An ancestor in a taskwait that must be unblocked at this point
			if (int err = nosv_submit(taskMetadata->getTaskHandle(), NOSV_SUBMIT_UNLOCKED))
				ErrorHandler::fail("nosv_submit failed: ", nosv_get_error_string(err));

			ready = false;
		}

		// Using 'task' here is forbidden, as it may have been disposed
		if (ready && parentMetadata != nullptr) {
			ready = parentMetadata->finishChild();
		}

		taskMetadata = parentMetadata;
	}

	if (localHpDependencyData != nullptr) {
		delete localHpDependencyData;
	}
}

void TaskFinalization::disposeTask(TaskMetadata *task)
{
	// Follow up the chain of ancestors and dispose them as needed and wake up
	// any in a taskwait that finishes in this moment
	bool disposable = true;
	TaskMetadata *taskMetadata = task;
	TaskMetadata *parentMetadata = nullptr;
	while ((taskMetadata != nullptr) && disposable) {
		parentMetadata = taskMetadata->getParent();
		if (parentMetadata != nullptr) {
			assert(taskMetadata->hasFinished());

			// Check if we continue the chain with the parent
			disposable = parentMetadata->decreaseRemovalBlockingCount();
		} else {
			disposable = (taskMetadata->getRemovalCount() == 0);
		}

		// Call the taskinfo destructor if not null
		nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(taskMetadata);
		assert(taskInfo != nullptr);

		if (taskInfo->destroy_args_block != nullptr) {
			taskInfo->destroy_args_block(taskMetadata->getArgsBlock());
		}

		if (taskMetadata->isSpawned()) {
			SpawnFunction::_pendingSpawnedFunctions--;
		}

		// Fetch the handle now as the metadata may get deleted
		nosv_task_t taskHandle = taskMetadata->getTaskHandle();

		// If the metadata was allocated locally, free it now
		if (taskMetadata->isLocallyAllocated()) {
			size_t metadataSize = taskMetadata->getTaskMetadataSize();
			MemoryAllocator::free(taskMetadata, metadataSize);
		}

		// Destroy the task
		if (int err = nosv_destroy(taskHandle, NOSV_DESTROY_NONE))
			ErrorHandler::fail("nosv_destroy failed: ", nosv_get_error_string(err));

		// Follow the chain of ancestors
		taskMetadata = parentMetadata;
	}
}
