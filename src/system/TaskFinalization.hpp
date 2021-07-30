/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_FINALIZATION_HPP
#define TASK_FINALIZATION_HPP

#include <nosv.h>


class TaskFinalization {

public:

	//! \brief Actions to take after a task has finished
	static void taskFinished(nosv_task_t task);

};

#endif // TASK_FINALIZATION_HPP
