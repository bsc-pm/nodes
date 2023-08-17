/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKITER_CHILD_METADATA_HPP
#define TASKITER_CHILD_METADATA_HPP

#include <cassert>
#include <cmath>

#include <nosv.h>

#include <nodes/loop.h>

#include "TaskMetadata.hpp"
#include "common/ErrorHandler.hpp"
#include "common/MathSupport.hpp"
#include "dependencies/discrete/taskiter/TaskiterGraph.hpp"
#include "dependencies/discrete/taskiter/TaskiterNode.hpp"

class TaskiterChildMetadata : public TaskMetadata, public TaskiterNode {

	public:
	inline TaskiterChildMetadata(
		void *argsBlock,
		size_t argsBlockSize,
		nosv_task_t taskPointer,
		size_t flags,
		const TaskDataAccessesInfo &taskAccessInfo,
		size_t taskMetadataSize,
		bool locallyAllocated
	) :
		TaskMetadata(argsBlock, argsBlockSize, taskPointer, flags, taskAccessInfo, taskMetadataSize, locallyAllocated),
		TaskiterNode((TaskMetadata *) this, nullptr)
	{
	}

	bool hasCommunication() const override
	{
		return this->isCommunicationTask();
	}

	bool isTaskiterChild() const override
	{
		return true;
	}

	virtual ~TaskiterChildMetadata() = default;
};

#endif // TASKITER_CHILD_METADATA_HPP
