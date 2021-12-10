/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include "HardwareInfo.hpp"


size_t HardwareInfo::_numCpus(0);
CPUDependencyData *HardwareInfo::_cpuDepDataArray;
