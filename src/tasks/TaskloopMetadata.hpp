/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKLOOP_METADATA_HPP
#define TASKLOOP_METADATA_HPP

#include <cmath>

#include <api/loop.h>

#include "TaskMetadata.hpp"
#include "common/MathSupport.hpp"


class TaskloopMetadata : public TaskMetadata {

public:

	typedef nanos6_loop_bounds_t bounds_t;

private:

	bounds_t _bounds;

	bool _source;

	// In some cases, the compiler cannot precisely indicate the number of deps.
	// In these cases, it passes -1 to the runtime so the deps are dynamically
	// registered. We have a loop where the parent registers all the deps of the
	// child tasks. We can count on that loop how many deps has each child and
	// get the max, so when we create children, we can use a more refined
	// numDeps, saving memory space and probably improving slightly the performance
	size_t _maxChildDeps;

public:

	inline TaskloopMetadata(
		void *argsBlock,
		size_t argsBlockSize,
		size_t flags,
		const TaskDataAccessesInfo &taskAccessInfo
	) :
		TaskMetadata(argsBlock, argsBlockSize, flags, taskAccessInfo),
		_bounds(),
		_source(false),
		_maxChildDeps(0)
	{
	}

	inline void initialize(size_t lowerBound, size_t upperBound, size_t grainsize, size_t chunksize)
	{
		_bounds.lower_bound = lowerBound;
		_bounds.upper_bound = upperBound;
		_bounds.grainsize = grainsize;
		_bounds.chunksize = chunksize;
		_source = true;

		size_t totalIterations = getIterationCount();

		// Set a implementation defined chunksize if needed
		if (_bounds.grainsize == 0) {
			_bounds.grainsize = std::max(totalIterations / nosv_get_num_cpus(), (size_t) 1);
		}
	}

	inline bounds_t &getBounds()
	{
		return _bounds;
	}

	inline bounds_t const &getBounds() const
	{
		return _bounds;
	}

	inline size_t getMaxChildDependencies() const
	{
		return _maxChildDeps;
	}

	inline void increaseMaxChildDependencies() override
	{
		if (_source) {
			_maxChildDeps++;
		}
	}

	inline bool isTaskloopSource() const override
	{
		return _source;
	}

	inline size_t getIterationCount() const
	{
		return (_bounds.upper_bound - _bounds.lower_bound);
	}

	void registerDependencies(nosv_task_t task) override;

};

namespace Taskloop {

	static inline size_t computeNumTasks(size_t iterations, size_t grainsize)
	{
		if (grainsize == 0) {
			grainsize = std::max(iterations / nosv_get_num_cpus(), (size_t) 1);
		}
		return MathSupport::ceil(iterations, grainsize);
	}

	static inline void createTaskloopExecutor(
		nosv_task_t parent,
		TaskloopMetadata *parentMetadata,
		TaskloopMetadata::bounds_t &parentBounds
	) {
		assert(parentMetadata != nullptr);

		// Retreive the args block and taskinfo of the task
		nosv_task_type_t type = nosv_get_task_type(parent);
		nanos6_task_info_t *parentTaskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(parentTaskInfo != nullptr);

		size_t flags = parentMetadata->getFlags();
		void *originalArgsBlock = parentMetadata->getArgsBlock();
		size_t originalArgsBlockSize = parentMetadata->getArgsBlockSize();
		bool hasPreallocatedArgsBlock = parentMetadata->hasPreallocatedArgsBlock();

		void *argsBlock = nullptr;
		if (hasPreallocatedArgsBlock) {
			assert(parentTaskInfo->duplicate_args_block != nullptr);

			parentTaskInfo->duplicate_args_block(originalArgsBlock, &argsBlock);
		}

		// This number has been computed while registering the parent's dependencies
		size_t numDeps = parentMetadata->getMaxChildDependencies();
		void *taskPointer = nullptr;
		nanos6_create_task(
			parentTaskInfo,
			nullptr,
			originalArgsBlockSize,
			&argsBlock,
			&taskPointer,
			flags,
			numDeps
		);
		assert(taskPointer != nullptr);

		TaskloopMetadata *taskloopMetadata = (TaskloopMetadata *) nosv_get_task_metadata((nosv_task_t) taskPointer);
		argsBlock = taskloopMetadata->getArgsBlock();
		assert(argsBlock != nullptr);

		// Copy the args block if it was not duplicated
		if (!hasPreallocatedArgsBlock) {
			if (parentTaskInfo->duplicate_args_block != nullptr) {
				parentTaskInfo->duplicate_args_block(originalArgsBlock, &argsBlock);
			} else {
				memcpy(argsBlock, originalArgsBlock, originalArgsBlockSize);
			}
		}

		// Set bounds of grainsize
		size_t lowerBound = parentBounds.lower_bound;
		size_t upperBound = std::min(lowerBound + parentBounds.grainsize, parentBounds.upper_bound);
		parentBounds.lower_bound = upperBound;

		TaskloopMetadata::bounds_t &childBounds = taskloopMetadata->getBounds();
		childBounds.lower_bound = lowerBound;
		childBounds.upper_bound = upperBound;

		// Submit task and register dependencies
		nanos6_submit_task(taskPointer);
	}

};

#endif // TASKLOOP_METADATA_HPP
