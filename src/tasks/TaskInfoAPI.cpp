/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <nodes/task-info-registration.h>

#include "tasks/TaskInfo.hpp"


extern "C" void nanos6_register_task_info(nanos6_task_info_t *task_info)
{
	TaskInfo::registerTaskInfo(task_info);
}
