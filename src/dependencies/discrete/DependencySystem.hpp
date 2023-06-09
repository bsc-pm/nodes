/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef DEPENDENCY_SYSTEM_HPP
#define DEPENDENCY_SYSTEM_HPP

#include <cassert>
#include <cstddef>

#include "CPUDependencyData.hpp"
#include "common/MathSupport.hpp"
#include "hardware/HardwareInfo.hpp"


class DependencySystem {

public:

	static void initialize()
	{
		size_t pow2CPUs = MathSupport::roundToNextPowOf2(HardwareInfo::getNumCpus());
		TaskList::_actualChunkSize = std::min(TaskList::getMaxChunkSize(), pow2CPUs * 2);
		assert(MathSupport::isPowOf2(TaskList::_actualChunkSize));
	}
};

#endif // DEPENDENCY_SYSTEM_HPP
