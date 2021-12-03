/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include "TaskloopMetadata.hpp"


void TaskloopMetadata::registerDependencies()
{
	// Retreive the args block and taskinfo of the task
	nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(_task);
	assert(taskInfo != nullptr);

	if (isTaskloopSource()) {
		bounds_t tmpBounds;
		size_t numTasks = Taskloop::computeNumTasks(getIterationCount(), _bounds.grainsize);
		for (size_t t = 0; t < numTasks; t++) {
			// Store previous maxChildDeps
			size_t maxChildDepsStart = _maxChildDeps;

			// Reset
			_maxChildDeps = 0;

			// Register deps of children task
			tmpBounds.lower_bound = _bounds.lower_bound + t * _bounds.grainsize;
			tmpBounds.upper_bound = std::min(tmpBounds.lower_bound + _bounds.grainsize, _bounds.upper_bound);

			taskInfo->register_depinfo(getArgsBlock(), (void *) &tmpBounds, this);

			// Restore previous maxChildDeps if it is bigger than current one
			if (maxChildDepsStart > _maxChildDeps) {
				_maxChildDeps = maxChildDepsStart;
			}
		}
		assert(tmpBounds.upper_bound == _bounds.upper_bound);
	} else {
		taskInfo->register_depinfo(getArgsBlock(), (void *) &_bounds, this);
	}
}
