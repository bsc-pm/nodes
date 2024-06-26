/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef RELEASE_DIRECTIVE_HPP
#define RELEASE_DIRECTIVE_HPP

#include <nodes/multidimensional-release.h>

#include "DataAccessRegistration.hpp"
#include "hardware/HardwareInfo.hpp"
#include "tasks/TaskMetadata.hpp"


template <DataAccessType ACCESS_TYPE, bool WEAK>
void release_access(void *base_address, __attribute__((unused)) long dim1size, long dim1start, __attribute__((unused)) long dim1end)
{
	TaskMetadata *task = TaskMetadata::getCurrentTask();
	assert(task != nullptr);

	int cpuId = nosv_get_current_logical_cpu();
	if (cpuId < 0) {
		ErrorHandler::fail("nosv_get_current_logical_cpu failed: ", nosv_get_error_string(cpuId));
	}

	CPUDependencyData *cpuDepData = HardwareInfo::getCPUDependencyData(cpuId);
	void *effectiveAddress = static_cast<char *>(base_address) + dim1start;

	DataAccessRegistration::releaseAccessRegion(task, effectiveAddress, ACCESS_TYPE, WEAK, cpuId, *cpuDepData);
}

void nanos6_release_read_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<READ_ACCESS_TYPE, false>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_write_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<WRITE_ACCESS_TYPE, false>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_readwrite_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<READWRITE_ACCESS_TYPE, false>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_concurrent_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<CONCURRENT_ACCESS_TYPE, false>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_commutative_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<COMMUTATIVE_ACCESS_TYPE, false>(base_address, dim1size, dim1start, dim1end);
}


void nanos6_release_weak_read_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<READ_ACCESS_TYPE, true>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_weak_write_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<WRITE_ACCESS_TYPE, true>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_weak_readwrite_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<READWRITE_ACCESS_TYPE, true>(base_address, dim1size, dim1start, dim1end);
}

void nanos6_release_weak_commutative_1(void *base_address, long dim1size, long dim1start, long dim1end)
{
	release_access<COMMUTATIVE_ACCESS_TYPE, true>(base_address, dim1size, dim1start, dim1end);
}

#endif // RELEASE_DIRECTIVE_HPP
