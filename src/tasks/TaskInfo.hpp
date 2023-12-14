/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2024 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_INFO_HPP
#define TASK_INFO_HPP

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include <nosv.h>

#include <nodes/task-instantiation.h>

#include "common/Chrono.hpp"
#include "common/ErrorHandler.hpp"
#include "common/SpinLock.hpp"
#include "dependencies/SymbolTranslation.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "memory/MemoryAllocator.hpp"
#include "system/TaskFinalization.hpp"
#include "tasks/TaskiterMetadata.hpp"
#include "tasks/TaskloopMetadata.hpp"
#include "tasks/TaskMetadata.hpp"


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

	// Global number of unlabeled task infos
	static size_t unlabeledTaskInfos;

public:

	//! \brief Run wrapper for task types
	static inline void runWrapper(nosv_task_t task)
	{
		assert(task != nullptr);

		nosv_task_t lastTask = TaskMetadata::getLastTask();
		TaskMetadata::setLastTask(task);

		nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(task);
		assert(taskInfo != nullptr);
		assert(taskInfo->implementation_count == 1);
		assert(taskInfo->implementations != nullptr);

		TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
		Chrono chrono;
		if (taskMetadata->isTaskiterChild())
			chrono.start();

		if (taskMetadata->hasCode()) {
			size_t tableSize = 0;
			int cpuId = nosv_get_current_logical_cpu();
			if (cpuId < 0) {
				ErrorHandler::fail("nosv_get_current_logical_cpu failed: ", nosv_get_error_string(cpuId));
			}

			nanos6_address_translation_entry_t stackTranslationTable[SymbolTranslation::MAX_STACK_SYMBOLS];
			nanos6_address_translation_entry_t *translationTable = SymbolTranslation::generateTranslationTable(
				task, cpuId, stackTranslationTable, tableSize
			);

			const bool isTaskiterChild = taskMetadata->getParent() && taskMetadata->getParent()->isTaskiter();

			if (taskMetadata->isTaskloop()) {
				TaskloopMetadata *taskloopMetadata = (TaskloopMetadata *) taskMetadata;
				if (!taskloopMetadata->isTaskloopSource()) {
					taskInfo->implementations->run(
						taskloopMetadata->getArgsBlock(),
						&(taskloopMetadata->getBounds()),
						translationTable
					);
				} else {
					if (!isTaskiterChild)
						taskloopMetadata->generateChildTasks();
				}
			} else if (taskMetadata->isTaskiter()) {
				TaskiterMetadata *taskiterMetadata = (TaskiterMetadata *)taskMetadata;
				for (size_t i = 0; i < taskiterMetadata->getUnroll(); ++i) {
					if (i > 0)
						taskiterMetadata->unrolledOnce();

					taskInfo->implementations->run(
						taskMetadata->getArgsBlock(),
						nullptr, /* deviceEnvironment */
						translationTable
					);
				}
			} else {
				taskInfo->implementations->run(
					taskMetadata->getArgsBlock(),
					nullptr, /* deviceEnvironment */
					translationTable
				);
			}

			// Free up all symbol translation
			if (tableSize > 0) {
				MemoryAllocator::free(translationTable, tableSize);
			}
		}

		if (taskMetadata->isTaskiterChild()) {
			chrono.stop();

			int cpuId = nosv_get_current_system_cpu();
			if (cpuId < 0) {
				ErrorHandler::fail("nosv_get_current_system_cpu failed: ", nosv_get_error_string(cpuId));
			}

			taskMetadata->setElapsedTime(chrono);
			taskMetadata->setLastExecutionCore(cpuId);
		}

		if (!taskMetadata->isIf0Inlined()) {
			assert(taskMetadata->getParent() != nullptr);

			// If the task is if0, it means the parent task was blocked. In this
			// case, unblock the parent at the end of the task's execution
			if (int err = nosv_submit(taskMetadata->getParent()->getTaskHandle(), NOSV_SUBMIT_UNLOCKED))
				ErrorHandler::fail("nosv_submit failed: ", nosv_get_error_string(err));
		}

		TaskMetadata::setLastTask(lastTask);
	}

	//! \brief Initialize the TaskInfo manager after the runtime has been initialized
	static inline void initialize()
	{
		_initialized = true;

		_lock.lock();

		for (size_t i = 0; i < _taskInfos.size(); ++i) {
			createTaskType(_taskInfos[i]);
		}

		_lock.unlock();
	}

	//! \brief Wrapper to obtain the cost of a task
	static inline uint64_t getCostWrapper(nosv_task_t task)
	{
		assert(task != nullptr);

		nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(task);
		if (taskInfo != nullptr) {
			if (taskInfo->implementations != nullptr) {
				if (taskInfo->implementations->get_constraints != nullptr) {
					TaskMetadata *taskMetadata = TaskMetadata::getTaskMetadata(task);
					assert(taskMetadata != nullptr);

					nanos6_task_constraints_t constraints;
					taskInfo->implementations->get_constraints(taskMetadata->getArgsBlock(), &constraints);
					return constraints.cost;
				}
			}
		}

		return 1;
	}

	//! \brief Register the taskinfo of a type of task
	//!
	//! \param[in,out] taskInfo A pointer to the taskinfo
	static void registerTaskInfo(nanos6_task_info_t *taskInfo);

	//! \brief Unregister all the registered taskinfos
	static void shutdown();

	//! \brief Create a nOS-V task type from a taskinfo and link them
	//!
	//! \param[in] taskInfo A pointer to the taskinfo
	static inline nosv_task_type_t createTaskType(nanos6_task_info_t *taskInfo)
	{
		std::string label;
		if (taskInfo->implementations->task_type_label) {
			label = taskInfo->implementations->task_type_label;
		} else {
			label = "Unlabeled" + std::to_string(unlabeledTaskInfos++);
		}

		// Create the task type
		nosv_task_type_t type;
		int err = nosv_type_init(
			&type,                                      /* Out: The pointer to the type */
			&(TaskInfo::runWrapper),                    /* Run callback wrapper for the tasks */
			&(TaskFinalization::taskEndedCallback),     /* End callback for when a task completes user code execution */
			&(TaskFinalization::taskCompletedCallback), /* Completed callback for when a task completely finishes */
			label.c_str(),                              /* Task type label */
			(void *) taskInfo,                          /* Metadata: Link to NODES' taskinfo */
			&(TaskInfo::getCostWrapper),
			NOSV_TYPE_INIT_NONE
		);
		if (err)
			ErrorHandler::fail("nosv_type_init failed: ", nosv_get_error_string(err));

		// Link the taskinfo to the task type
		taskInfo->task_type_data = (void *) type;

		// Save the task type to destroy during the finalization
		_taskTypes.push_back(type);

		return type;
	}

};

#endif // TASK_INFO_HPP
