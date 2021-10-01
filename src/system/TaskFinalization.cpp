/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>

#include <nosv.h>

#include <api/task-instantiation.h>

#include "TaskFinalization.hpp"
#include "dependencies/discrete/CPUDependencyData.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "hardware/HardwareInfo.hpp"
#include "system/SpawnFunction.hpp"
#include "system/TaskCreation.hpp"


void TaskFinalization::taskCompletedCallback(nosv_task_t task)
{
	int cpuId = nosv_get_current_system_cpu();
	DataAccessRegistration::combineTaskReductions(task, cpuId);
}

void TaskFinalization::taskEndedCallback(nosv_task_t task)
{
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Mark that the task has finished user code execution
	taskMetadata->markAsFinished();

	// If the task has a wait clause, the release of dependencies must be
	// delayed (at least) until the task finishes its execution and all
	// its children complete and become disposable
	bool dependenciesReleaseable = true;
	if (taskMetadata->mustDelayRelease()) {
		DataAccessRegistration::handleEnterTaskwait(task);
		if (!taskMetadata->markAsBlocked()) {
			dependenciesReleaseable = false;
		} else {
			// All its children are completed, so the delayed release as well
			taskMetadata->completeDelayedRelease();
			DataAccessRegistration::handleExitTaskwait(task);
			taskMetadata->markAsUnblocked();
		}
	}

	if (dependenciesReleaseable) {
		dependenciesReleaseable = taskMetadata->decreaseReleaseCount();
	}

	// Check whether all external events have been also fulfilled, so the dependencies can be released
	if (dependenciesReleaseable) {
		// We always use a local CPUDependencyData struct here to avoid re-using issues
		CPUDependencyData *localHpDependencyData = new CPUDependencyData();
		DataAccessRegistration::unregisterTaskDataAccesses(task, *localHpDependencyData);

		TaskFinalization::taskFinished(task);

		if (taskMetadata->decreaseRemovalBlockingCount()) {
			TaskFinalization::disposeTask(task);
		}

		delete localHpDependencyData;
	}
}

void TaskFinalization::taskFinished(nosv_task_t task)
{
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Decrease the _countdownToBeWokenUp of the task, which was initialized to 1
	// If it becomes 0, we can propagate the counter through its parents
	bool ready = taskMetadata->finishChild();

	// NOTE: Needed?
	// We always use a local CPUDependencyData struct here to avoid issues
	// with re-using an already used CPUDependencyData
	CPUDependencyData *localHpDependencyData = nullptr;
	nosv_task_t parent = nullptr;
	while ((task != nullptr) && ready) {
		parent = taskMetadata->_parent;

		// If this is the first iteration of the loop, the task will test true
		// to hasFinished and false to mustDelayRelease, which does nothing
		if (taskMetadata->hasFinished()) {
			if (taskMetadata->mustDelayRelease()) {
				taskMetadata->completeDelayedRelease();
				DataAccessRegistration::handleExitTaskwait(task);
				taskMetadata->markAsUnblocked();

				// Check whether all external events have been also fulfilled
				bool dependenciesReleaseable = taskMetadata->decreaseReleaseCount();
				if (dependenciesReleaseable) {
					if (localHpDependencyData == nullptr) {
						localHpDependencyData = new CPUDependencyData();
					}

					DataAccessRegistration::unregisterTaskDataAccesses(task, *localHpDependencyData);

					// This is just to emulate a recursive call to TaskFinalization::taskFinished() again
					// It should not return false because at this point delayed release has happenned which means that
					// the task has gone through a taskwait (no more children should be unfinished)
					ready = taskMetadata->finishChild();
					assert(ready);

					if (taskMetadata->decreaseRemovalBlockingCount()) {
						TaskFinalization::disposeTask(task);
					}
				}

				assert(!taskMetadata->mustDelayRelease());
			}
		} else {
			// An ancestor in a taskwait that must be unblocked at this point
			nosv_submit(task, NOSV_SUBMIT_UNLOCKED);

			ready = false;
		}

		// Using 'task' here is forbidden, as it may have been disposed
		if (ready && parent != nullptr) {
			taskMetadata = (TaskMetadata *) nosv_get_task_metadata(parent);
			assert(taskMetadata != nullptr);

			ready = taskMetadata->finishChild();
		}

		task = parent;
	}

	if (localHpDependencyData != nullptr) {
		delete localHpDependencyData;
	}
}

void TaskFinalization::disposeTask(nosv_task_t task)
{
	TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
	assert(taskMetadata != nullptr);

	// Follow up the chain of ancestors and dispose them as needed and wake up
	// any in a taskwait that finishes in this moment
	bool disposable = true;
	nosv_task_t parent = nullptr;
	TaskMetadata *parentMetadata = nullptr;
	while ((task != nullptr) && disposable) {
		parent = taskMetadata->_parent;
		if (parent != nullptr) {
			parentMetadata = (TaskMetadata *) nosv_get_task_metadata(parent);
			assert(taskMetadata->hasFinished());

			// Check if we continue the chain with the parent
			disposable = parentMetadata->decreaseRemovalBlockingCount();
		}

		// Call the taskinfo destructor if not null
		nosv_task_type_t type = nosv_get_task_type(task);
		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);

		if (taskInfo->destroy_args_block != nullptr) {
			taskInfo->destroy_args_block(taskMetadata->_argsBlock);
		}

		if (taskMetadata->isSpawned()) {
			SpawnFunction::_pendingSpawnedFunctions--;
		}

		// Destroy the task
		nosv_destroy(task, NOSV_DESTROY_NONE);

		// Follow the chain of ancestors
		task = parent;
		taskMetadata = parentMetadata;
	}
}

