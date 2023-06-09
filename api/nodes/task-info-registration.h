/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_TASK_INFO_REGISTRATION_H
#define NODES_TASK_INFO_REGISTRATION_H

#include "major.h"
#include "task-instantiation.h"


#pragma GCC visibility push(default)

enum nanos6_task_info_registration_api_t { nanos6_task_info_registration_api = 2 };

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Register a type of task
//!
//! \param[in] task_info a pointer to the nanos6_task_info_t structure
void nanos6_register_task_info(nanos6_task_info_t *task_info);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* NODES_TASK_INFO_REGISTRATION_H */
