/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <nosv.h>

#include <api/bootstrap.h>
#include <api/taskwait.h>

#include "dependencies/discrete/DependencySystem.hpp"
#include "hardware/HardwareInfo.hpp"
#include "system/SpawnFunction.hpp"
#include "tasks/TaskInfo.hpp"


void nanos6_init(void)
{
	// Initialize nOS-V backend
	nosv_init();

	// Gather hardware info
	HardwareInfo::initialize();

	// Initialize the TaskInfo manager after nOS-V has been initialized
	TaskInfo::initialize();

	// Initialize the dependency system
	DependencySystem::initialize();
}

void nanos6_shutdown(void)
{
	// TODO: nosv_sleep
	while (SpawnFunction::_pendingSpawnedFunctions > 0) {
		// Wait for spawned functions to fully end
	}

	// Unregister any registered taskinfo from nOS-V
	TaskInfo::shutdown();

	// Shutdown nOS-V backend
	nosv_shutdown();

	// Shutdown hardware info
	HardwareInfo::shutdown();
}
