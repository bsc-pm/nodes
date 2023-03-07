/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cstdlib>
#include <unordered_set>

#include "common/ErrorHandler.hpp"
#include "system/TaskCreation.hpp"
#include "tasks/TaskInfo.hpp"
#include "tasks/TaskiterMetadata.hpp"
#include "tasks/TaskiterChildMetadata.hpp"

// For posix_memalign
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

nanos6_task_invocation_info_t functionInvocationInfo = {"Automatically inserted due to a while-taskiter"};

struct TaskiterArgsBlock {
	TaskiterMetadata *taskiter;
};

void TaskiterMetadata::controlCallback(void *args, void *, nanos6_address_translation_entry_t *)
{
	TaskiterArgsBlock *argsBlock = (TaskiterArgsBlock *) args;
	TaskiterMetadata *taskiter = argsBlock->taskiter;
	uint8_t evaluation = 0;
	taskiter->_iterationCondition(argsBlock->taskiter->getArgsBlock(), &evaluation);

	if (!evaluation) {
		// Stop the execution of the taskiter
		// However, we have to delay its cancellation if we're unrolling a taskiter

		taskiter->activateDelayedCancellation();
	}

	if (taskiter->shouldCancel()) {
		taskiter->cancel();
	}
}

void TaskiterMetadata::cancel()
{
	assert(isWhile());

	_stop = true;

	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(currentTask);
	// We have to finish the groups also

	std::unordered_set<TaskGroupMetadata *> groups;

	_graph.forEach(
		[taskMetadata, &groups](TaskMetadata *task) {
			if (task != taskMetadata) {
				if (task->getGroup() != nullptr) {
					groups.insert((TaskGroupMetadata *) task->getGroup());
				}

				// As this is the second time this task will be finished, we have to do a little hack
				if (task->canBeWokenUp())
					task->increaseWakeUpCount(1);
				TaskFinalization::taskFinished(task);

				bool deletable = task->decreaseRemovalBlockingCount();

				if (deletable) {
					TaskFinalization::disposeTask(task);
				}
			}
		},
		true
	);

	for (TaskGroupMetadata *group : groups) {
		if (group->canBeWokenUp())
			group->increaseWakeUpCount(1);
		TaskFinalization::taskFinished(group);

		bool deletable = group->decreaseRemovalBlockingCount();

		if (deletable) {
			TaskFinalization::disposeTask(group);
		}
	}
}

TaskMetadata *TaskiterMetadata::generateControlTask()
{
	assert(isWhile());

	// TODO Free this memory
	nanos6_task_info_t *taskInfo = nullptr;
	int err = posix_memalign((void **) &taskInfo, 64, sizeof(nanos6_task_info_t));
	ErrorHandler::failIf(err != 0, " when allocating memory with posix_memalign");
	assert(taskInfo != nullptr);

	// Ensure non-explicitely initialized fields are zeroed
	memset(taskInfo, 0, sizeof(nanos6_task_info_t));

	taskInfo->implementations =
		(nanos6_task_implementation_info_t *)malloc(sizeof(nanos6_task_implementation_info_t));
	assert(taskInfo->implementations != nullptr);

	taskInfo->implementation_count = 1;
	taskInfo->implementations[0].run = TaskiterMetadata::controlCallback;
	taskInfo->implementations[0].device_type_id = nanos6_device_t::nanos6_host_device;
	taskInfo->register_depinfo = nullptr;

	// The completion callback will be called when the task is destroyed
	taskInfo->destroy_args_block = nullptr;

	// Use a copy since we do not know the actual lifetime of label
	taskInfo->implementations[0].task_type_label = "Taskiter Control";
	taskInfo->implementations[0].declaration_source = "Taskiter Control";
	taskInfo->implementations[0].get_constraints = nullptr;

	taskInfo->get_priority = NULL;

	taskInfo->num_symbols = 0;

	// Register the new task info
	TaskInfo::registerTaskInfo(taskInfo);

	// Create the task representing the spawned function
	void *task = nullptr;
	TaskiterArgsBlock *argsBlock = nullptr;
	TaskCreation::createTask<TaskiterChildMetadata>(taskInfo, NULL, "Taskiter Control", sizeof(TaskiterArgsBlock), (void **)&argsBlock, &task, 0, 0);
	assert(task != nullptr);
	assert(argsBlock != nullptr);

	argsBlock->taskiter = this;

	TaskMetadata *metadata = TaskMetadata::getTaskMetadata((nosv_task_t) task);
	metadata->setParent(this->getTaskHandle());
	metadata->incrementOriginalPredecessorCount();
	metadata->setIterationCount(getIterationCount() + 1);

	return metadata;
}
