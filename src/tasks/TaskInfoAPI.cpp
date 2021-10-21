/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nanos6/task-info-registration.h>

#include "tasks/TaskInfo.hpp"


extern "C" void nanos6_register_task_info(nanos6_task_info_t *task_info)
{
	TaskInfo::registerTaskInfo(task_info);
}
