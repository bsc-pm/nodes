/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <api/taskwait.h>


//! \brief Block the control flow of the current task until all of its children have finished
//!
//! \param[in] invocationSource A string that identifies the source code location of the invocation
extern "C" void nanos6_taskwait(char const */*invocationSource*/)
{
	// TODO: Wait for all tasks to end execution
	// Block (nosv_pause?)
}
