/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef HARDWARE_INFO_HPP
#define HARDWARE_INFO_HPP

#include <cstddef>
#include <sched.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <boost/dynamic_bitset.hpp>

#include "dependencies/discrete/CPUDependencyData.hpp"


class HardwareInfo {

private:

	//! Number of available cpus
	static size_t _numCpus;

	//! Total number of CPUs in the system
	static size_t _totalNumCpus;

	//! The process' CPU mask
	static cpu_set_t _cpuMask;

	//! An array of CPU dependency data indexed by CPU ids
	static CPUDependencyData *_cpuDepDataArray;

public:

	static inline void initialize()
	{
		// Initialize mask to zeros
		CPU_ZERO(&_cpuMask);

		// Retreive the CPU mask of this process
		int rc = sched_getaffinity(0, sizeof(cpu_set_t), &_cpuMask);
		ErrorHandler::handle(rc, " when retrieving the affinity of the process");

		_numCpus = CPU_COUNT(&_cpuMask);

		// Get the maximum number of CPUs in the system
		_totalNumCpus = get_nprocs_conf();

		_cpuDepDataArray = (CPUDependencyData *) malloc(sizeof(CPUDependencyData) * _totalNumCpus);
		assert(_cpuDepDataArray != nullptr);

		// Initialize each CPUDependencyData
		for (size_t i = 0; i < _totalNumCpus; ++i) {
			new (&(_cpuDepDataArray[i])) CPUDependencyData();
		}
	}

	static inline void shutdown()
	{
		assert(_cpuDepDataArray != nullptr);

		free(_cpuDepDataArray);
	}

	static inline size_t getNumCpus()
	{
		return _numCpus;
	}

	static inline size_t getTotalNumCpus()
	{
		return _totalNumCpus;
	}

	static inline CPUDependencyData *getCPUDependencyData(size_t cpuId)
	{
		assert(cpuId < _totalNumCpus);

		return &(_cpuDepDataArray[cpuId]);
	}

};

#endif // HARDWARE_INFO_HPP
