/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2022 Barcelona Supercomputing Center (BSC)
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
	size_t _lowerBound;
	size_t _upperBound;
	size_t _unroll;
	std::function<void(void *, uint8_t *)> _iterationCondition;
	TaskiterGraph _graph;
	TaskMetadata *_controlTask;
	bool _stop;

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
		_lowerBound(0),
		_upperBound(0),
		_unroll(1),
		_controlTask(nullptr),
		_stop(false)
	{
		// Delay dependency release
		setDelayedRelease(true);
	}

	inline void initialize(size_t lowerBound, size_t upperBound, size_t unroll, std::function<void(void *, uint8_t *)> iterationCondition)
	{
		_lowerBound = lowerBound;
		_upperBound = upperBound;
		_unroll = unroll;
		_iterationCondition = iterationCondition;

		if (iterationCondition != nullptr) {
			// Registering a taskiter + while
			assert(_lowerBound == 0);
			assert(_upperBound == 1);
			_upperBound = (size_t)-1;
		}
	}

	inline size_t getIterationCount() const
	{
		return (_upperBound - _lowerBound);
	}

	inline bool isTaskiter() const override
	{
		return true;
	}

	inline bool isWhile() const
	{
		return _iterationCondition != nullptr;
	}

	inline bool evaluateCondition()
	{
		uint8_t conditionVariable;
		_iterationCondition(getArgsBlock(), &conditionVariable);

		return (bool)conditionVariable;
	}

	inline TaskiterGraph &getGraph()
	{
		return _graph;
	}

	TaskMetadata *generateControlTask();

	static void controlCallback(void *args, void *, nanos6_address_translation_entry_t *);

	void cancel();

	inline bool cancelled() const
	{
		return _stop;
	}
};

#endif // TASKITER_METADATA_HPP
