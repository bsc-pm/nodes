/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <mutex>
#include <utility>

#include <nosv.h>

#include "SpawnFunction.hpp"
#include "instrument/OVNIInstrumentation.hpp"
#include "tasks/TaskInfo.hpp"


//! Static members
std::atomic<unsigned int> SpawnFunction::_pendingSpawnedFunctions(0);
std::map<SpawnFunction::task_info_key_t, nanos6_task_info_t> SpawnFunction::_spawnedFunctionInfos;
SpinLock SpawnFunction::_spawnedFunctionInfosLock;
nanos6_task_invocation_info_t SpawnFunction::_spawnedFunctionInvocationInfo = { "Spawned from external code" };


//! Args block of spawned functions
struct SpawnedFunctionArgsBlock {
	SpawnFunction::function_t _function;
	void *_args;
	SpawnFunction::function_t _completionCallback;
	void *_completionArgs;

	SpawnedFunctionArgsBlock() :
		_function(nullptr),
		_args(nullptr),
		_completionCallback(nullptr),
		_completionArgs(nullptr)
	{
	}
};


void nanos6_spawn_function(
	void (*function)(void *),
	void *args,
	void (*completion_callback)(void *),
	void *completion_args,
	char const *label
) {
	SpawnFunction::spawnFunction(function, args, completion_callback, completion_args, label, true);
}

void SpawnFunction::spawnedFunctionWrapper(void *args, void *, nanos6_address_translation_entry_t *)
{
	SpawnedFunctionArgsBlock *argsBlock = (SpawnedFunctionArgsBlock *) args;
	assert(argsBlock != nullptr);

	// Call the user spawned function
	argsBlock->_function(argsBlock->_args);
}

void SpawnFunction::spawnedFunctionDestructor(void *args)
{
	SpawnedFunctionArgsBlock *argsBlock = (SpawnedFunctionArgsBlock *) args;
	assert(argsBlock != nullptr);

	// Call the user completion callback if present
	if (argsBlock->_completionCallback != nullptr) {
		argsBlock->_completionCallback(argsBlock->_completionArgs);
	}
}

void SpawnFunction::spawnFunction(
	function_t function,
	void *args,
	function_t completionCallback,
	void *completionArgs,
	char const *label,
	bool fromUserCode
) {
	Instrument::enterSpawnFunction();

	// Increase the number of spawned functions in case it is
	// spawned from outside the runtime system
	if (fromUserCode) {
		_pendingSpawnedFunctions++;
	}

	nanos6_task_info_t *taskInfo = nullptr;
	{
		task_info_key_t taskInfoKey(function, (label != nullptr ? label : ""));

		std::lock_guard<SpinLock> guard(_spawnedFunctionInfosLock);
		auto itAndBool = _spawnedFunctionInfos.emplace(
			std::make_pair(taskInfoKey, nanos6_task_info_t())
		);
		auto it = itAndBool.first;
		taskInfo = &(it->second);

		if (itAndBool.second) {
			// New task info
			taskInfo->implementations = (nanos6_task_implementation_info_t *)
				malloc(sizeof(nanos6_task_implementation_info_t));
			assert(taskInfo->implementations != nullptr);

			taskInfo->implementation_count = 1;
			taskInfo->implementations[0].run = SpawnFunction::spawnedFunctionWrapper;
			taskInfo->implementations[0].device_type_id = nanos6_device_t::nanos6_host_device;
			taskInfo->register_depinfo = nullptr;

			// The completion callback will be called when the task is destroyed
			taskInfo->destroy_args_block = SpawnFunction::spawnedFunctionDestructor;

			// Use a copy since we do not know the actual lifetime of label
			taskInfo->implementations[0].task_type_label = it->first.second.c_str();
			taskInfo->implementations[0].declaration_source = "Spawned Task";
			taskInfo->implementations[0].get_constraints = nullptr;
		}
	}

	// Register the new task info
	TaskInfo::registerTaskInfo(taskInfo);

	// Create the task representing the spawned function
	void *task = nullptr;
	SpawnedFunctionArgsBlock *argsBlock = nullptr;
	nanos6_create_task(
		taskInfo, &_spawnedFunctionInvocationInfo, nullptr,
		sizeof(SpawnedFunctionArgsBlock),
		(void **) &argsBlock, &task, nanos6_waiting_task, 0
	);
	assert(task != nullptr);
	assert(argsBlock != nullptr);

	argsBlock->_function = function;
	argsBlock->_args = args;
	argsBlock->_completionCallback = completionCallback;
	argsBlock->_completionArgs = completionArgs;

	// Set the task as spawned
	TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata((nosv_task_t) task);
	taskMetadata->setSpawned(true);

	// Submit the task
	nanos6_submit_task(task);

	Instrument::exitSpawnFunction();
}

