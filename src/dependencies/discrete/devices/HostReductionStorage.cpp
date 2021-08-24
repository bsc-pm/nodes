/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nosv.h>

#include "HostReductionStorage.hpp"
#include "hardware/HardwareInfo.hpp"
#include "memory/MemoryAllocator.hpp"


HostReductionStorage::HostReductionStorage(void *address, size_t length, size_t paddedLength,
	std::function<void(void *, void *, size_t)> initializationFunction,
	std::function<void(void *, void *, size_t)> combinationFunction) :
	DeviceReductionStorage(address, length, paddedLength, initializationFunction, combinationFunction),
	_freeSlotIndices(HardwareInfo::getTotalNumCpus())
{
	const long nCpus = HardwareInfo::getTotalNumCpus();
	assert(nCpus > 0);

	// Create all slots
	_slots.resize(nCpus);
	_currentCpuSlotIndices.resize(nCpus, -1);
}

void *HostReductionStorage::getFreeSlotStorage(__attribute__((unused)) nosv_task_t task, size_t slotIndex, size_t)
{
	assert(task != nullptr);
	assert(slotIndex < _slots.size());

	slot_t &slot = _slots[slotIndex];
	assert(slot.initialized || slot.storage == nullptr);

	if (!slot.initialized) {
		// Allocate new storage
		slot.storage = MemoryAllocator::alloc(_paddedLength);
		_initializationFunction(slot.storage, _address, _length);
		slot.initialized = true;
	}

	return slot.storage;
}

void HostReductionStorage::combineInStorage(void *combineDestination)
{
	assert(combineDestination != nullptr);

	// Ensure we see writes from other threads that affected the slots
	std::atomic_thread_fence(std::memory_order_acquire);
	for (size_t i = 0; i < _slots.size(); ++i) {
		slot_t &slot = _slots[i];

		if (slot.initialized) {
			assert(slot.storage != nullptr);
			assert(slot.storage != combineDestination);

			_combinationFunction(combineDestination, slot.storage, _length);

			MemoryAllocator::free(slot.storage, _paddedLength);
			slot.storage = nullptr;
			slot.initialized = false;
		}
	}
}

size_t HostReductionStorage::getFreeSlotIndex(nosv_task_t, size_t cpuId)
{
	assert((size_t) cpuId < _currentCpuSlotIndices.size());
	long int currentSlotIndex = _currentCpuSlotIndices[cpuId];

	if (currentSlotIndex != -1) {
		// Storage already assigned to this CPU
		// Note: Currently, this can only happen with a weakreduction task with
		// 2 or more (in_final) reduction subtasks that will be requesting storage
		// Note: Task scheduling points within reduction are currently not supported,
		// as tied tasks are not yet implemented. If supported, task counters would be
		// required to avoid the storage to be released at the end of a task while still in use

		assert(_slots[currentSlotIndex].initialized);
		return currentSlotIndex;
	}

	int freeSlotIndex = _freeSlotIndices.setFirst();
	while (freeSlotIndex == -1)
		freeSlotIndex = _freeSlotIndices.setFirst();

	_currentCpuSlotIndices[cpuId] = freeSlotIndex;

	return freeSlotIndex;
}

void HostReductionStorage::releaseSlotsInUse(nosv_task_t, size_t cpuId)
{
	assert(cpuId < _currentCpuSlotIndices.size());
	long int currentSlotIndex = _currentCpuSlotIndices[cpuId];

	// Note: If access is weak and final (promoted), but had no reduction subtasks, this
	// member can be called when _currentCpuSlotIndices[task] is invalid (hasn't been used)
	if (currentSlotIndex != -1) {
		assert(_slots[currentSlotIndex].storage != nullptr);
		assert(_slots[currentSlotIndex].initialized);
		_freeSlotIndices.reset(currentSlotIndex);
		_currentCpuSlotIndices[cpuId] = -1;
	}
}
