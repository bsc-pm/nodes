/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "TaskInfo.hpp"


std::vector<nosv_task_type_t> TaskInfo::_taskTypes;
std::vector<nanos6_task_info_t *> TaskInfo::_taskInfos;
SpinLock TaskInfo::_lock;
bool TaskInfo::_initialized;


void TaskInfo::registerTaskInfo(nanos6_task_info_t *taskInfo)
{
	assert(taskInfo != nullptr);
	assert(taskInfo->implementations != nullptr);
	assert(taskInfo->implementation_count == 1);

	__attribute__((unused)) nanos6_device_t deviceType = (nanos6_device_t) taskInfo->implementations->device_type_id;
	assert(deviceType == nanos6_host_device);

	_lock.lock();

	if (_initialized) {
		// Create the task type
		nosv_task_type_t type;
		int ret = nosv_type_init(
			&type,                                      /* Out: The pointer to the type */
			&(TaskInfo::runWrapper),                    /* Run callback wrapper for the tasks */
			&(TaskFinalization::taskEndedCallback),     /* End callback for when a task completes user code execution */
			&(TaskFinalization::taskCompletedCallback), /* Completed callback for when a task completely finishes */
			taskInfo->implementations->task_label,      /* Task label */
			(void *) taskInfo,                          /* Metadata: Link to nODES' taskinfo */
			NOSV_TYPE_INIT_NONE
		);
		assert(!ret);

		// Save a nOS-V type link in the task info
		taskInfo->task_type_data = (void *) type;

		_taskTypes.push_back(type);
	} else {
		_taskInfos.push_back(taskInfo);
	}

	_lock.unlock();
}

void TaskInfo::shutdown()
{
	_lock.lock();
	for (size_t i = 0; i < _taskTypes.size(); ++i) {
		nosv_type_destroy(_taskTypes[i], NOSV_TYPE_DESTROY_NONE);
	}
	_lock.unlock();
}
