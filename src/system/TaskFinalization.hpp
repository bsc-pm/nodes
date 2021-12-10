/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_FINALIZATION_HPP
#define TASK_FINALIZATION_HPP

#include <nosv.h>


class TaskMetadata;

class TaskFinalization {

public:

	//! \brief Actions to take after a task ends its user-code execution
	static void taskEndedCallback(nosv_task_t task);

	//! \brief Actions to take after a task completes user-code execution
	static void taskCompletedCallback(nosv_task_t task);

	//! \brief Further actions (wrapper) to take after a task has finished
	static void taskFinished(TaskMetadata *task);

	//! \brief Dispose of a task
	static void disposeTask(TaskMetadata *task);

};

#endif // TASK_FINALIZATION_HPP
