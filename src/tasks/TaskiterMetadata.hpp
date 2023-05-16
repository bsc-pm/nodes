/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_METADATA_HPP
#define TASKITER_METADATA_HPP

#include <cassert>
#include <cmath>

#include <nosv.h>

#include <nodes/loop.h>

#include "TaskMetadata.hpp"
#include "common/ErrorHandler.hpp"
#include "common/MathSupport.hpp"
#include "dependencies/discrete/taskiter/TaskiterGraph.hpp"

class TaskiterMetadata : public TaskMetadata {
	size_t _lowerBound;
	size_t _upperBound;
	size_t _unroll;
	std::function<void(void *, uint8_t *)> _iterationCondition;
	TaskiterGraph _graph;
	size_t _delayedCancelCountdown;
	bool _delayedCancel;
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
		_delayedCancelCountdown(0),
		_delayedCancel(false),
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

		// assert(unroll > 0);
		// TODO: The compiler should deliver a default unroll of 1...
		if (_unroll == 0)
			_unroll = 1;

		if (unroll > 1 && !iterationCondition) {
			// Perform some sanity checks and adapt the number of iterations accordingly
			size_t cnt = upperBound - lowerBound;
			ErrorHandler::failIf(unroll > cnt, "Cannot unroll taskiter more times than loop iterations");
			ErrorHandler::failIf((cnt % unroll) != 0, "The number of taskiter iterations must be a multiple of its unroll factor");

			_upperBound = lowerBound + cnt / unroll;
		}

		_iterationCondition = iterationCondition;

		if (iterationCondition != nullptr) {
			// Registering a taskiter + while
			// We set _upperBound to size_t's max. This simplifies the condition checking for the taskiter while
			// As we can do (--_iterationCount > 0) and it will never reach 0.
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

	inline size_t getUnroll() const
	{
		return _unroll;
	}

	inline void unrolledOnce()
	{
		// If we have a condition, this is a strided taskiter
		// Hence, we will have to notify the graph that a logical "iteration" has happenned to
		// insert a control task

		if (isWhile()) {
			_graph.insertControlInUnrolledLoop(generateControlTask());
			__attribute__((unused)) bool finished = this->finishChild();
			assert(!finished);
		}
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

	inline void activateDelayedCancellation()
	{
		if (_delayedCancel)
			return;

		_delayedCancel = true;
		_delayedCancelCountdown = _unroll;
	}

	inline bool shouldCancel()
	{
		return _delayedCancel && (--_delayedCancelCountdown) == 0;
	}

	inline bool isCancellationDelayed() const
	{
		return _delayedCancel;
	}
};

#endif // TASKITER_METADATA_HPP
