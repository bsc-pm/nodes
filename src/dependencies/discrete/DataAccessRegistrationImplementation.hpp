/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_REGISTRATION_IMPLEMENTATION_HPP
#define DATA_ACCESS_REGISTRATION_IMPLEMENTATION_HPP

#include <mutex>

#include <nosv.h>

#include "DataAccessRegistration.hpp"
#include "TaskDataAccesses.hpp"


namespace DataAccessRegistration {

	template <typename ProcessorType>
	inline bool processAllDataAccesses(nosv_task_t task, ProcessorType processor)
	{
		assert(task != nullptr);

		// Retreive the task's metadata
		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		return accessStruct.forAll(
			[&](void *, DataAccess *access) -> bool {
				return processor(access);
			}
		);
	}
}

#endif // DATA_ACCESS_REGISTRATION_IMPLEMENTATION_HPP
