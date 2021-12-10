/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef HOST_REDUCTION_STORAGE_HPP
#define HOST_REDUCTION_STORAGE_HPP

#include "common/AtomicBitset.hpp"
#include "dependencies/discrete/DeviceReductionStorage.hpp"
#include "tasks/TaskMetadata.hpp"


class HostReductionStorage : public DeviceReductionStorage {

public:

	struct ReductionSlot {
		void *storage = nullptr;
		bool initialized = false;
	};

	typedef ReductionSlot slot_t;

	HostReductionStorage(void *address, size_t length, size_t paddedLength,
		std::function<void(void *, void *, size_t)> initializationFunction,
		std::function<void(void *, void *, size_t)> combinationFunction);

	void *getFreeSlotStorage(TaskMetadata *task, size_t slotIndex, size_t cpuId);

	void combineInStorage(void *combineDestination);

	void releaseSlotsInUse(TaskMetadata *task, size_t cpuId);

	size_t getFreeSlotIndex(TaskMetadata *task, size_t cpuId);

	~HostReductionStorage(){};

private:

	std::vector<slot_t> _slots;
	std::vector<long int> _currentCpuSlotIndices;
	AtomicBitset<> _freeSlotIndices;

};

#endif // HOST_REDUCTION_STORAGE_HPP
