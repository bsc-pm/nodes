/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_CREATION_HPP
#define TASK_CREATION_HPP

#include <nodes/task-instantiation.h>

#include <nosv.h>

class TaskCreation {

public:
	//! \brief Create a NODES task
	template <typename T>
	static void createTask(
		nanos6_task_info_t *taskInfo,
		nanos6_task_invocation_info_t *,
		char const *,
		size_t argsBlockSize,
		void **argsBlockPointer,
		void **taskPointer,
		size_t flags,
		size_t numDeps);

	//! \brief Submit a NODES task
	static void submitTask(nosv_task_t task);
};

#endif // TASK_CREATION_HPP
