/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <api/bootstrap.h>
#include <api/taskwait.h>

#include "SpawnFunction.hpp"
#include "tasks/TaskInfo.hpp"


void nanos6_init(void)
{
	// Initialize nOS-V backend
	nosv_init();

	// Initialize the TaskInfo manager after nOS-V has been initialized
	TaskInfo::initialize();
}

void nanos6_shutdown(void)
{
	while (SpawnFunction::_pendingSpawnedFunctions > 0) {
		// Wait for spawned functions to fully end
	}

	// Unregister any registered taskinfo from nOS-V
	TaskInfo::shutdown();

	// Shutdown nOS-V backend
	nosv_shutdown();
}
