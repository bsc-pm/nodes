/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef REDUCTION_INFO_HPP
#define REDUCTION_INFO_HPP

#include <atomic>
#include <functional>

#include <nodes/task-instantiation.h>

#include "ReductionSpecific.hpp"
#include "common/Containers.hpp"
#include "common/PaddedSpinLock.hpp"
#include "dependencies/DataAccessRegion.hpp"


class DeviceReductionStorage;
class TaskMetadata;

class ReductionInfo {

public:

	typedef PaddedSpinLock<> spinlock_t;

private:

	DataAccessRegion _region;

	void *_address;

	const size_t _length;

	const size_t _paddedLength;

	reduction_type_and_operator_index_t _typeAndOperatorIndex;

	DeviceReductionStorage *_deviceStorages[nanos6_device_type_num];

	std::function<void(void *, void *, size_t)> _initializationFunction;
	std::function<void(void *, void *, size_t)> _combinationFunction;

	std::atomic<size_t> _registeredAccesses;

	// Keep track of the originally registered accesses in case this reduction in re-used
	std::atomic<size_t> _originalAccesses;

	const bool _inTaskiter;

	spinlock_t _lock;

	DeviceReductionStorage *allocateDeviceStorage(nanos6_device_t deviceType);

public:

	ReductionInfo(void *address, size_t length, reduction_type_and_operator_index_t typeAndOperatorIndex,
		std::function<void(void *, void *, size_t)> initializationFunction,
		std::function<void(void *, void *, size_t)> combinationFunction, bool inTaskiter);

	virtual ~ReductionInfo();

	inline reduction_type_and_operator_index_t getTypeAndOperatorIndex() const
	{
		return _typeAndOperatorIndex;
	}

	inline const void *getOriginalAddress() const
	{
		return _address;
	}

	size_t getOriginalLength() const
	{
		return _length;
	}

	void combine();

	void releaseSlotsInUse(TaskMetadata *task, size_t cpuId);

	void *getFreeSlot(TaskMetadata *task, size_t cpuId);

	inline void reinitialize()
	{
		_registeredAccesses.store(_originalAccesses.load());
	}

	inline bool isInTaskiter()
	{
		return _inTaskiter;
	}

	inline void incrementRegisteredAccesses()
	{
		++_registeredAccesses;
	}

	inline void incrementOriginalRegisteredAccesses()
	{
		++_originalAccesses;
	}

	inline bool incrementUnregisteredAccesses()
	{
		assert(_registeredAccesses > 0);
		return (--_registeredAccesses == 0);
	}

	inline bool markAsClosed()
	{
		return incrementUnregisteredAccesses();
	}

	bool finished()
	{
		return (_registeredAccesses == 0);
	}

	const DataAccessRegion &getOriginalRegion() const
	{
		return _region;
	}
};

#endif // REDUCTION_INFO_HPP
