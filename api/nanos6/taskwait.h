/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_TASKWAIT_H
#define NODES_TASKWAIT_H

#include "major.h"


#pragma GCC visibility push(default)

enum nanos6_taskwait_api_t { nanos6_taskwait_api = 3 };

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Block the control flow of the current task until all of its children have finished
//!
//! \param[in] invocation_source A string that identifies the source code location of the invocation
void nanos6_taskwait(char const *invocation_source);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif // NODES_TASKWAIT_H
