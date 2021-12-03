/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_BOOTSTRAP_H
#define NANOS6_BOOTSTRAP_H

#include <stddef.h>

#include "major.h"


#pragma GCC visibility push(default)

enum nanos6_bootstrap_api_t { nanos6_bootstrap_api = 2 };

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Continue with the rest of the runtime initialization
void nanos6_init(void);

//! \brief Force the runtime to be shut down
void nanos6_shutdown(void);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* NANOS6_BOOTSTRAP_H */
