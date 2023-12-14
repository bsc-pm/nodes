/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2024 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "TaskInfo.hpp"
#include "common/ErrorHandler.hpp"


std::vector<nosv_task_type_t> TaskInfo::_taskTypes;
std::vector<nanos6_task_info_t *> TaskInfo::_taskInfos;
SpinLock TaskInfo::_lock;
bool TaskInfo::_initialized;
size_t TaskInfo::unlabeledTaskInfos = 0;

void TaskInfo::registerTaskInfo(nanos6_task_info_t *taskInfo)
{
	assert(taskInfo != nullptr);
	assert(taskInfo->implementations != nullptr);
	assert(taskInfo->implementation_count == 1);

	__attribute__((unused)) nanos6_device_t deviceType = (nanos6_device_t) taskInfo->implementations->device_type_id;
	assert(deviceType == nanos6_host_device);

	_lock.lock();

	if (_initialized) {
		createTaskType(taskInfo);
	} else {
		_taskInfos.push_back(taskInfo);
	}

	_lock.unlock();
}

void TaskInfo::shutdown()
{
	_lock.lock();
	for (size_t i = 0; i < _taskTypes.size(); ++i) {
		if (int err = nosv_type_destroy(_taskTypes[i], NOSV_TYPE_DESTROY_NONE))
			ErrorHandler::fail("nosv_type_destroy failed: ", nosv_get_error_string(err));
	}
	_lock.unlock();
}
