/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DEVICE_REDUCTION_STORAGE_HPP
#define DEVICE_REDUCTION_STORAGE_HPP

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

#include "ReductionInfo.hpp"
#include "ReductionSpecific.hpp"
#include "dependencies/DataAccessRegion.hpp"
#include "tasks/TaskMetadata.hpp"


class DeviceReductionStorage {

protected:

	void *_address;
	const size_t _length;
	const size_t _paddedLength;

	std::function<void(void *, void *, size_t)> _initializationFunction;
	std::function<void(void *, void *, size_t)> _combinationFunction;

public:

	DeviceReductionStorage(
		void *address, size_t length, size_t paddedLength,
		std::function<void(void *, void *, size_t)> initializationFunction,
		std::function<void(void *, void *, size_t)> combinationFunction) :
		_address(address),
		_length(length),
		_paddedLength(paddedLength),
		_initializationFunction(initializationFunction),
		_combinationFunction(combinationFunction)
	{
	}

	virtual ~DeviceReductionStorage()
	{
	}

	virtual void releaseSlotsInUse(TaskMetadata *task, size_t cpuId) = 0;

	virtual size_t getFreeSlotIndex(TaskMetadata *task, size_t cpuId) = 0;

	virtual void *getFreeSlotStorage(TaskMetadata *task, size_t slotIndex, size_t cpuId) = 0;

	virtual void combineInStorage(void *combineDestination) = 0;
};

#endif // DEVICE_REDUCTION_STORAGE_HPP
