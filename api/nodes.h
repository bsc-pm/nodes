/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2024 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_H
#define NODES_H

#ifdef __cplusplus
#include <new>
#endif

#define __NODES__


#include "nodes/blocking.h"
#include "nodes/bootstrap.h"
#include "nodes/constants.h"
#include "nodes/debug.h"
#include "nodes/dependencies.h"
#include "nodes/events.h"
#include "nodes/final.h"
#include "nodes/library-mode.h"
#include "nodes/loop.h"
#include "nodes/major.h"
#include "nodes/multidimensional-dependencies.h"
#include "nodes/multidimensional-release.h"
#include "nodes/reductions.h"
#include "nodes/task-info-registration.h"
#include "nodes/task-instantiation.h"
#include "nodes/taskwait.h"
#include "nodes/user-mutex.h"
#include "nodes/version.h"

#if __cplusplus >= 202002L
#include "nodes/coroutines.hpp"
#endif

#endif /* NODES_H */
