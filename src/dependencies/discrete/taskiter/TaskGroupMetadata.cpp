/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2023-2024 Barcelona Supercomputing Center (BSC)
*/

#include "system/TaskFinalization.hpp"
#include "tasks/TaskInfo.hpp"
#include "TaskGroupMetadata.hpp"

static nanos6_task_implementation_info_t groupTaskInfoImplementation = {
	.device_type_id = nanos6_device_t::nanos6_host_device,
	.run = TaskGroupMetadata::executeTask,
	.get_constraints = nullptr,
	.task_type_label = "Task Group",
	.declaration_source = "Task Group",
	.run_wrapper = nullptr
};

static nanos6_task_info_t groupTaskInfo = {
	.num_symbols = 0,
	.register_depinfo = nullptr,
	.onready_action = nullptr,
	.get_priority = nullptr,
	.implementation_count = 1,
	.implementations = &groupTaskInfoImplementation,
	.destroy_args_block = nullptr,
	.duplicate_args_block = nullptr,
	.reduction_initializers = nullptr,
	.reduction_combiners = nullptr,
	.task_type_data = nullptr,
	.iter_condition = nullptr,
	.num_args = 0,
	.sizeof_table = nullptr,
	.offset_table = nullptr,
	.arg_idx_table = nullptr,
	.coro_handle_idx = -1
};

nanos6_task_info_t *TaskGroupMetadata::getGroupTaskInfo()
{
	static bool registered = false;

	if (!registered) {
		TaskInfo::registerTaskInfo(&groupTaskInfo);
		registered = true;
	}

	return &groupTaskInfo;
}

void TaskGroupMetadata::executeTask(void *args, void *, nanos6_address_translation_entry_t *)
{
	auto visitor = overloaded {
		[](ReductionInfo *reductionInfo) {
			reductionInfo->combine();
			reductionInfo->reinitialize();
		},
		[](TaskMetadata *arg) {
			nosv_task_t t = arg->getTaskHandle();
			TaskInfo::runWrapper(t);
			TaskFinalization::taskEndedCallback(t);
		}
	};

	TaskGroupMetadata **group = (TaskGroupMetadata **)args;
	for (TaskiterNode *task : (*group)->_tasksInGroup) {
		task->apply(visitor);
	}
}

void TaskGroupMetadata::mergeWithGroup(TaskGroupMetadata *group)
{
	for (TaskiterNode *n : group->_tasksInGroup)
		addTask(n);

	// Remove the group?
	group->markAsFinished();
	[[maybe_unused]] bool ready = group->getParent()->finishChild();
	assert(!ready);
	TaskFinalization::disposeTask(group);
}