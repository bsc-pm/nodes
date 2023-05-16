/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_FINAL_H
#define NODES_FINAL_H

#include "major.h"


#pragma GCC visibility push(default)

enum nanos6_final_api_t { nanos6_final_api = 1 };

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Check if running in a final context
signed int nanos6_in_final(void);

//! \brief Check if running in a serial context
signed int nanos6_in_serial_context(void);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif // NODES_FINAL_H
