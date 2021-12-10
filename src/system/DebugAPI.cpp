/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cstddef>

#include <nosv.h>

#include <nanos6/debug.h>


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
	return nosv_get_current_system_cpu();
}

extern "C" unsigned int nanos6_get_current_virtual_cpu(void)
{
	return nosv_get_current_logical_cpu();
}

