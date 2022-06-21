/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include <iostream>

#include "tasks/TaskInfo.hpp"
#include "tasks/TaskiterMetadata.hpp"

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

	_graph.forEach(
		[](TaskMetadata *task) {
			// As this is the second time this task will be finished, we have to do a little hack
			task->addChilds(1);
			TaskFinalization::taskFinished(task);

			__attribute__((unused)) bool deletable = task->decreaseRemovalBlockingCount();
			// assert(deletable);
			if (deletable) {
				TaskFinalization::disposeTask(task);
			}
		},
		true
	);
}

TaskMetadata *TaskiterMetadata::generateControlTask()
{
	assert(isWhile());

	// TODO Free this memory
	nanos6_task_info_t *taskInfo = (nanos6_task_info_t *)aligned_alloc(64, sizeof(nanos6_task_info_t));
	assert(taskInfo != nullptr);
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

	// Register the new task info
	TaskInfo::registerTaskInfo(taskInfo);

	// Create the task representing the spawned function
	void *task = nullptr;
	TaskiterArgsBlock *argsBlock = nullptr;
	nanos6_create_task(
		taskInfo, &functionInvocationInfo,
		"Taskiter Control", sizeof(TaskiterArgsBlock),
		(void **)&argsBlock, &task, 0, 0);
	assert(task != nullptr);
	assert(argsBlock != nullptr);

	argsBlock->taskiter = this;

	TaskMetadata *metadata = TaskMetadata::getTaskMetadata((nosv_task_t) task);
	metadata->setParent(this->getTaskHandle());
	metadata->incrementOriginalPredecessorCount();
	metadata->setIterationCount(getIterationCount() + 1);
	metadata->markAsBlocked();

	return metadata;
}
