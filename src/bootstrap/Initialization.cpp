/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <nanos6/bootstrap.h>
#include <nanos6/taskwait.h>

#include "dependencies/discrete/DependencySystem.hpp"
#include "hardware/HardwareInfo.hpp"
#include "system/SpawnFunction.hpp"
#include "tasks/TaskInfo.hpp"


void nanos6_init(void)
{
	// Initialize nOS-V backend
	nosv_init();

	// Create a dummy type and task to attach/wrap the "main" process
	nosv_task_t task;
	nosv_task_type_t type;
	nosv_type_init(&type, NULL, NULL, NULL, "main", NULL, NOSV_TYPE_INIT_EXTERNAL);
	nosv_attach(&task, type, 0, NULL, NOSV_ATTACH_NONE);

	// Gather hardware info
	HardwareInfo::initialize();

	// Initialize the TaskInfo manager after nOS-V has been initialized
	TaskInfo::initialize();

	// Initialize the dependency system
	DependencySystem::initialize();
}

void nanos6_shutdown(void)
{
	// TODO: Add nosv_sleep while waiting
	while (SpawnFunction::_pendingSpawnedFunctions > 0) {
		// Wait for spawned functions to fully end
	}

	// Unregister any registered taskinfo from nOS-V
	TaskInfo::shutdown();

	// Shutdown hardware info
	HardwareInfo::shutdown();

	// Detach the wrapped main process
	nosv_detach(NOSV_DETACH_NONE);

	// Shutdown nOS-V backend
	nosv_shutdown();
}
