/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cstddef>

#include <nosv.h>

#include <nodes/debug.h>

#include "common/ErrorHandler.hpp"


extern "C" unsigned int nanos6_get_total_num_cpus(void)
{
	return nosv_get_num_cpus();
}

extern "C" unsigned int nanos6_get_num_cpus(void)
{
	return nosv_get_num_cpus();
}

extern "C" long nanos6_get_current_system_cpu(void)
{
	int cpuId = nosv_get_current_system_cpu();
	if (cpuId < 0) {
		ErrorHandler::fail("nosv_get_current_system_cpu failed: ", nosv_get_error_string(cpuId));
	}

	return cpuId;
}

extern "C" unsigned int nanos6_get_current_virtual_cpu(void)
{
	int cpuId = nosv_get_current_logical_cpu();
	if (cpuId < 0) {
		ErrorHandler::fail("nosv_get_current_logical_cpu failed: ", nosv_get_error_string(cpuId));
	}

	return cpuId;
}
