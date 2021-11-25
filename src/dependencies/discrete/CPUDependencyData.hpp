/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef CPU_DEPENDENCY_DATA_HPP
#define CPU_DEPENDENCY_DATA_HPP

#include <atomic>
#include <cassert>
#include <limits.h>

#include <nanos6/task-instantiation.h>

#include "DataAccessFlags.hpp"
#include "common/Containers.hpp"
#include "tasks/TaskMetadata.hpp"


class SatisfiedOriginatorList {

public:

	static size_t _actualChunkSize;

private:

	static const size_t _schedulerChunkSize = 256;

	TaskMetadata *_array[_schedulerChunkSize];

	size_t _count;

public:

	inline SatisfiedOriginatorList() :
		_count(0)
	{
	}

	inline size_t size() const
	{
		return _count;
	}

	inline void clear()
	{
		_count = 0;
	}

	inline void add(TaskMetadata *task)
	{
		assert(_count < _actualChunkSize);

		_array[_count++] = task;
	}

	inline TaskMetadata **getArray()
	{
		return &_array[0];
	}

	static inline size_t getMaxChunkSize()
	{
		return _schedulerChunkSize;
	}
};

struct CPUDependencyData {

	typedef SatisfiedOriginatorList satisfied_originator_list_t;
	typedef Container::deque<TaskMetadata *> commutative_satisfied_list_t;
	typedef Container::deque<TaskMetadata *> deletable_originator_list_t;

	//! Tasks whose accesses have been satisfied after ending a task
	satisfied_originator_list_t _satisfiedOriginators[nanos6_device_t::nanos6_device_type_num];
	size_t _satisfiedOriginatorCount;

	deletable_originator_list_t _deletableOriginators;
	commutative_satisfied_list_t _satisfiedCommutativeOriginators;
	mailbox_t _mailBox;

#ifndef NDEBUG
	std::atomic<bool> _inUse;
#endif

	CPUDependencyData()
		: _satisfiedOriginators(),
		_satisfiedOriginatorCount(0),
		_deletableOriginators(),
		_satisfiedCommutativeOriginators(),
		_mailBox()
#ifndef NDEBUG
		, _inUse()
#endif
	{
	}

	~CPUDependencyData()
	{
		assert(empty());
	}

	inline bool empty() const
	{
		for (const satisfied_originator_list_t &list : _satisfiedOriginators)
			if (list.size() > 0)
				return false;

		return _deletableOriginators.empty() && _mailBox.empty() && _satisfiedCommutativeOriginators.empty();
	}

	inline void addSatisfiedOriginator(TaskMetadata *task, int deviceType = nanos6_host_device)
	{
		assert(task != nullptr);
		assert(deviceType == nanos6_host_device);
		assert(_satisfiedOriginatorCount < (size_t) satisfied_originator_list_t::_actualChunkSize);

		_satisfiedOriginatorCount++;
		_satisfiedOriginators[deviceType].add(task);
	}

	inline bool full() const
	{
		assert(satisfied_originator_list_t::_actualChunkSize != 0);

		return (_satisfiedOriginatorCount == satisfied_originator_list_t::_actualChunkSize);
	}

	inline satisfied_originator_list_t &getSatisfiedOriginators(int device)
	{
		return _satisfiedOriginators[device];
	}

	inline void clearSatisfiedOriginators()
	{
		for (satisfied_originator_list_t &list : _satisfiedOriginators) {
			list.clear();
		}

		_satisfiedOriginatorCount = 0;
	}
};

#endif // CPU_DEPENDENCY_DATA_HPP
