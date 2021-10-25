/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_INFO_HPP
#define TASK_INFO_HPP

#include <cassert>
#include <string>
#include <vector>

#include <nosv.h>

#include <nanos6/task-instantiation.h>

#include "common/SpinLock.hpp"
#include "dependencies/SymbolTranslation.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "memory/MemoryAllocator.hpp"
#include "system/TaskFinalization.hpp"
#include "tasks/TaskMetadata.hpp"
#include "tasks/TaskloopMetadata.hpp"


class TaskInfo {

private:

	//! A vector with nOS-V tasktype info pointers
	static std::vector<nosv_task_type_t> _taskTypes;

	//! A vector of unregistered nanos6 task infos to register after init
	static std::vector<nanos6_task_info_t *> _taskInfos;

	//! SpinLock to register taskinfos
	static SpinLock _lock;

	//! Whether the manager has been initialized (after the runtime)
	static bool _initialized;

private:

	//! \brief Run wrapper for task types
	static inline void runWrapper(nosv_task_t task)
	{
		assert(task != nullptr);

		nosv_task_type_t type = nosv_get_task_type(task);
		assert(type != nullptr);

		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);
		assert(taskInfo->implementation_count == 1);
		assert(taskInfo->implementations != nullptr);

		size_t tableSize = 0;
		int cpuId = nosv_get_current_logical_cpu();
		nanos6_address_translation_entry_t stackTranslationTable[SymbolTranslation::MAX_STACK_SYMBOLS];
		nanos6_address_translation_entry_t *translationTable = SymbolTranslation::generateTranslationTable(
			task, cpuId, stackTranslationTable, tableSize
		);

		TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
		if (taskMetadata->isTaskloop()) {
			TaskloopMetadata *taskloopMetadata = (TaskloopMetadata *) taskMetadata;
			if (!taskloopMetadata->isTaskloopSource()) {
				taskInfo->implementations->run(
					taskloopMetadata->getArgsBlock(),
					&(taskloopMetadata->getBounds()),
					translationTable
				);
			} else {
				while (taskloopMetadata->getIterationCount() > 0) {
					Taskloop::createTaskloopExecutor(task, taskloopMetadata, taskloopMetadata->getBounds());
				}
			}
		} else {
			taskInfo->implementations->run(
				taskMetadata->getArgsBlock(),
				nullptr, /* deviceEnvironment */
				translationTable
			);
		}

		if (!taskMetadata->isIf0Inlined()) {
			// If the task is if0, it means the parent task was blocked. In this
			// case, unblock the parent at the end of the task's execution
			nosv_submit(taskMetadata->getParent(), NOSV_SUBMIT_UNLOCKED);
		}

		// Free up all symbol translation
		if (tableSize > 0) {
			MemoryAllocator::free(translationTable, tableSize);
		}
	}

public:

	//! \brief Initialize the TaskInfo manager after the runtime has been initialized
	static inline void initialize()
	{
		_initialized = true;

		_lock.lock();

		for (size_t i = 0; i < _taskInfos.size(); ++i) {
			nanos6_task_info_t *taskInfo = _taskInfos[i];

			// Create the task type
			nosv_task_type_t type;
			int ret = nosv_type_init(
				&type,                                      /* Out: The pointer to the type */
				&(TaskInfo::runWrapper),                    /* Run callback wrapper for the tasks */
				&(TaskFinalization::taskEndedCallback),     /* End callback for when a task completes user code execution */
				&(TaskFinalization::taskCompletedCallback), /* Completed callback for when a task completely finishes */
				taskInfo->implementations->task_label,      /* Task label */
				(void *) taskInfo,                          /* Metadata: Link to nanos6lite taskinfo */
				NOSV_TYPE_INIT_NONE
			);
			assert(!ret);

			// Save a nOS-V type link in the task info
			taskInfo->task_type_data = (void *) type;

			_taskTypes.push_back(type);
		}

		_lock.unlock();
	}

	//! \brief Register the taskinfo of a type of task
	//!
	//! \param[in,out] taskInfo A pointer to the taskinfo
	static void registerTaskInfo(nanos6_task_info_t *taskInfo);

	//! \brief Unregister all the registered taskinfos
	static void shutdown();

};

#endif // TASK_INFO_HPP
