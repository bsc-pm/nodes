/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <mutex>

#include <nosv.h>

#include "CommutativeSemaphore.hpp"
#include "CPUDependencyData.hpp"
#include "DataAccessRegistration.hpp"
#include "TaskDataAccesses.hpp"
#include "tasks/TaskMetadata.hpp"


CommutativeSemaphore::lock_t CommutativeSemaphore::_lock;
CommutativeSemaphore::waiting_tasks_t CommutativeSemaphore::_waitingTasks;
CommutativeSemaphore::commutative_mask_t CommutativeSemaphore::_mask;

bool CommutativeSemaphore::registerTask(nosv_task_t task)
{
	// Retreive the task's metadata
	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
	TaskDataAccesses &accessStruct = taskMetadata->getTaskDataAccesses();
	const commutative_mask_t &mask = accessStruct._commutativeMask;
	assert(mask.any());

	std::lock_guard<lock_t> guard(_lock);
	if (maskIsCompatible(mask)) {
		maskRegister(mask);
		return true;
	}

	_waitingTasks.emplace_back(std::forward<nosv_task_t>(task));
	return false;
}

void CommutativeSemaphore::releaseTask(nosv_task_t task, CPUDependencyData &hpDependencyData)
{
	// Retreive the task's metadata
	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
	TaskDataAccesses &accessStruct = taskMetadata->getTaskDataAccesses();
	const commutative_mask_t &mask = accessStruct._commutativeMask;
	assert(mask.any());

	commutative_mask_t released;

	std::lock_guard<lock_t> guard(_lock);
	maskRelease(mask);

	waiting_tasks_t::iterator it = _waitingTasks.begin();

	while (it != _waitingTasks.end()) {
		nosv_task_t candidate = *it;

		// Retreive the task's metadata
		TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(candidate);
		TaskDataAccesses &candidateStruct = taskMetadata->getTaskDataAccesses();
		const commutative_mask_t &candidateMask = candidateStruct._commutativeMask;

		if (maskIsCompatible(candidateMask)) {
			maskRegister(candidateMask);
			hpDependencyData._satisfiedCommutativeOriginators.push_back(candidate);
			it = _waitingTasks.erase(it);

			// Keep track and cut off if we won't be releasing anything else.
			released |= (mask & candidateMask);
			if (released == mask)
				break;
		} else {
			++it;
		}
	}
}
