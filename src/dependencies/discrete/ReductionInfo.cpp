/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <sys/mman.h>

#include <nanos6/reductions.h>

#include "DeviceReductionStorage.hpp"
#include "ReductionInfo.hpp"
#include "common/Padding.hpp"
#include "dependencies/discrete/devices/HostReductionStorage.hpp"
#include "memory/MemoryAllocator.hpp"
#include "tasks/TaskMetadata.hpp"


ReductionInfo::ReductionInfo(void *address, size_t length, reduction_type_and_operator_index_t typeAndOperatorIndex,
	std::function<void(void *, void *, size_t)> initializationFunction, std::function<void(void *, void *, size_t)> combinationFunction) :
	_address(address),
	_length(length),
	_paddedLength(((length + CACHELINE_SIZE - 1) / CACHELINE_SIZE) * CACHELINE_SIZE),
	_typeAndOperatorIndex(typeAndOperatorIndex),
	_initializationFunction(initializationFunction),
	_combinationFunction(combinationFunction),
	_registeredAccesses(2)
{
	for (size_t i = 0; i < nanos6_device_type_num; ++i) {
		_deviceStorages[i] = nullptr;
	}
}

ReductionInfo::~ReductionInfo()
{
	assert(_registeredAccesses == 0);

	for (size_t i = 0; i < nanos6_device_type_num; ++i) {
		if (_deviceStorages[i] != nullptr) {
			delete _deviceStorages[i];
		}
	}
}

void ReductionInfo::combine()
{
	// This lock should be uncontended, because "combine" is only done once
	// when all accesses have freed their slots
	std::lock_guard<spinlock_t> guard(_lock);
	assert(_address != nullptr);

	for (size_t i = 0; i < nanos6_device_type_num; ++i) {
		if (_deviceStorages[i] != nullptr)
			_deviceStorages[i]->combineInStorage(_address);
	}
}

void ReductionInfo::releaseSlotsInUse(TaskMetadata *task, size_t cpuId)
{
	DeviceReductionStorage *storage = _deviceStorages[nanos6_host_device];
	assert(storage != nullptr);

	storage->releaseSlotsInUse(task, cpuId);
}

DeviceReductionStorage *ReductionInfo::allocateDeviceStorage(nanos6_device_t deviceType)
{
	assert(deviceType == nanos6_host_device);
	assert(_deviceStorages[deviceType] == nullptr);

	DeviceReductionStorage *storage = nullptr;
	switch (deviceType) {
		case nanos6_host_device:
			storage = new HostReductionStorage(_address, _length, _paddedLength,
				_initializationFunction, _combinationFunction);
			break;
		default:
			break;
	}

	assert(storage != nullptr);

	// Ensure all threads see the initialized reduction storage before setting the pointer
	std::atomic_thread_fence(std::memory_order_release);

	_deviceStorages[deviceType] = storage;
	return storage;
}

void *ReductionInfo::getFreeSlot(TaskMetadata *task, size_t cpuId)
{
	DeviceReductionStorage *storage = _deviceStorages[nanos6_host_device];
	if (storage == nullptr) {
		std::lock_guard<spinlock_t> guard(_lock);

		// Check again because of possible races
		if (_deviceStorages[nanos6_host_device] == nullptr)
			storage = allocateDeviceStorage(nanos6_host_device);
		else
			storage = _deviceStorages[nanos6_host_device];
	}
	assert(storage != nullptr);

	// Ensure if we see the value of storage as non null we can see
	// all the writes done by the other threads to initialize it.
	// This should not be needed because there is an address dependency
	// and a release on the initialization (providing release-consumer ordering)
	// However, better be safe than sorry.
	std::atomic_thread_fence(std::memory_order_acquire);

	size_t index = storage->getFreeSlotIndex(task, cpuId);

	void *privateStorage = storage->getFreeSlotStorage(task, index, cpuId);
	assert(privateStorage != nullptr);

	return privateStorage;
}
