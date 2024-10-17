/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef HARDWARE_INFO_HPP
#define HARDWARE_INFO_HPP

#include <cstddef>
#include <stdlib.h>
#include <unistd.h>

#include <nosv/hwinfo.h>

#include "dependencies/discrete/CPUDependencyData.hpp"


class HardwareInfo {

private:

	//! Number of available cpus
	static size_t _numCpus;

	//! An array of CPU dependency data indexed by CPU ids
	static CPUDependencyData *_cpuDepDataArray;

public:

	static inline void initialize()
	{
		_numCpus = nosv_get_num_cpus();

		_cpuDepDataArray = new CPUDependencyData[_numCpus];
		assert(_cpuDepDataArray != nullptr);
	}

	static inline void shutdown()
	{
		assert(_cpuDepDataArray != nullptr);

		delete[] _cpuDepDataArray;
	}

	static inline size_t getNumCpus()
	{
		return _numCpus;
	}

	static inline CPUDependencyData *getCPUDependencyData(size_t cpuId)
	{
		assert(cpuId < _numCpus);

		return &(_cpuDepDataArray[cpuId]);
	}

};

#endif // HARDWARE_INFO_HPP
