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
		TaskMetadata *metadata = task->getTask();
		if (metadata) {
			if (metadata->isGroup()) {
				mergeWithGroup((TaskGroupMetadata *) metadata);
			} else {
				metadata->setGroup(this);
				task->setVertex(getVertex());
				_tasksInGroup.push_back(task);
				this->setElapsedTime(this->getElapsedTime() + metadata->getElapsedTime());
			}
		} else {
			task->setVertex(getVertex());
			_tasksInGroup.push_back(task);
		}
	}

	void mergeWithGroup(TaskGroupMetadata *group);

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

	void setVertex(size_t vertex) override
	{
		TaskiterNode::setVertex(vertex);
		for (TaskiterNode *t : _tasksInGroup)
			t->setVertex(vertex);
	}

	bool isTaskiterChild() const override
	{
		return true;
	}

	static nanos6_task_info_t *getGroupTaskInfo();
};

#endif // TASK_GROUP_METADATA_HPP