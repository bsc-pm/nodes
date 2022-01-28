/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_METADATA_HPP
#define TASKITER_METADATA_HPP

#include <cmath>

#include <nosv.h>

#include <nanos6/loop.h>

#include "TaskMetadata.hpp"
#include "common/MathSupport.hpp"
#include "dependencies/discrete/taskiter/TaskiterGraph.hpp"

class TaskiterMetadata : public TaskMetadata {
	size_t _lower_bound;
	size_t _upper_bound;
	size_t _unroll;
	std::function<void(void *, uint8_t *)> _iterationCondition;
	TaskiterGraph _graph;

public:

	inline TaskiterMetadata(
		void *argsBlock,
		size_t argsBlockSize,
		nosv_task_t taskPointer,
		size_t flags,
		const TaskDataAccessesInfo &taskAccessInfo,
		size_t taskMetadataSize,
		bool locallyAllocated
	) :
		TaskMetadata(argsBlock, argsBlockSize, taskPointer, flags, taskAccessInfo, taskMetadataSize, locallyAllocated),
		_lower_bound(0),
		_upper_bound(0),
		_unroll(1)
	{
		// Delay dependency release
		setDelayedRelease(true);
	}

	inline void initialize(size_t lowerBound, size_t upperBound, size_t unroll, std::function<void(void *, uint8_t *)> iterationCondition)
	{
		_lower_bound = lowerBound;
		_upper_bound = upperBound;
		_unroll = unroll;
		_iterationCondition = iterationCondition;
	}

	inline size_t getIterationCount() const
	{
		return (_upper_bound - _lower_bound);
	}

	inline bool isTaskiter() const override
	{
		return true;
	}

	inline TaskiterGraph &getGraph()
	{
		return _graph;
	}
};

#endif // TASKITER_METADATA_HPP
