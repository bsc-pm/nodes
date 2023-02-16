/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_GROUP_METADATA_HPP
#define TASK_GROUP_METADATA_HPP

#include <iostream>

#include "system/TaskFinalization.hpp"
#include "TaskiterNode.hpp"
#include "tasks/TaskMetadata.hpp"

class TaskInfo;

class TaskGroupMetadata : public TaskMetadata, public TaskiterNode {
	std::vector<TaskiterNode *> _tasksInGroup;

public:
	inline TaskGroupMetadata(
		void *argsBlock,
		size_t argsBlockSize,
		nosv_task_t taskPointer,
		size_t flags,
		const TaskDataAccessesInfo &taskAccessInfo,
		size_t taskMetadataSize,
		bool locallyAllocated):
	TaskMetadata(argsBlock, argsBlockSize, taskPointer, flags, taskAccessInfo, taskMetadataSize, locallyAllocated),
	TaskiterNode(this, nullptr)
	{
	}

	void addTask(TaskiterNode *task) 
	{
		_tasksInGroup.push_back(task);
		TaskMetadata *metadata = task->getTask();
		if (metadata)
			metadata->setGroup(this);
	}

	void mergeWithGroup(TaskGroupMetadata *group)
	{
		for (TaskiterNode *n : group->_tasksInGroup)
			addTask(n);
	}

	static void executeTask(void *args, void *, nanos6_address_translation_entry_t *);

	void finalizeGroupedTasks()
	{
		for (TaskiterNode *node : _tasksInGroup) {
			TaskMetadata *t = node->getTask();
			if (t) {
				t->setIterationCount(1);
				TaskFinalization::taskCompletedCallback(t->getTaskHandle());
			}
		}
	}

	bool isGroup() const override
	{
		return true;
	}

	static nanos6_task_info_t *getGroupTaskInfo();
};

#endif // TASK_GROUP_METADATA_HPP