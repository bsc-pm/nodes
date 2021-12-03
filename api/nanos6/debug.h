/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_DEBUG_H
#define NODES_DEBUG_H

#include "major.h"


#pragma GCC visibility push(default)

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Get the total number of CPUs available to the runtime
unsigned int nanos6_get_total_num_cpus(void);

//! \brief Get the number of CPUs that were enabled when the program started
unsigned int nanos6_get_num_cpus(void);

//! \brief Get the operating system assigned identifier of the
//! CPU where the call to this function originated
long nanos6_get_current_system_cpu(void);

//! \brief Get a CPU identifier assigned to the CPU where the
//! call to this function originated that starts from 0 up to
//! nanos6_get_num_cpus() - 1
unsigned int nanos6_get_current_virtual_cpu(void);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop


#endif /* NODES_DEBUG_H */
