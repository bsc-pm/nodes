/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_REGISTRATION_HPP
#define DATA_ACCESS_REGISTRATION_HPP

#include <cstddef>

#include <nosv.h>

#include <nanos6/task-instantiation.h>

#include "CPUDependencyData.hpp"
#include "DataAccess.hpp"
#include "ReductionSpecific.hpp"
#include "dependencies/DataAccessType.hpp"


struct TaskDataAccesses;

namespace DataAccessRegistration {

	//! \brief creates a task data access taking into account repeated accesses but does not link it to previous accesses nor superaccesses
	//!
	//! \param[in,out] task the task that performs the access
	//! \param[in] accessType the type of access
	//! \param[in] weak whether access is weak or strong
	//! \param[in] address the starting address of the access
	//! \param[in] the length of the access
	//! \param[in] reductionTypeAndOperatorIndex an index that identifies the type and the operation of the reduction
	//! \param[in] reductionIndex an index that identifies the reduction within the task

	void registerTaskDataAccess(
		nosv_task_t task, DataAccessType accessType, bool weak, void *address, size_t length,
		reduction_type_and_operator_index_t reductionTypeAndOperatorIndex, reduction_index_t reductionIndex, int symbolIndex);

	//! \brief Performs the task dependency registration procedure
	//!
	//! \param[in] task the Task whose dependencies need to be calculated
	//!
	//! \returns true if the task is already ready
	bool registerTaskDataAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData);

	void unregisterTaskDataAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData);

	void releaseAccessRegion(
		nosv_task_t task, void * address,
		DataAccessType accessType,
		bool weak,
		size_t cpuId,
		CPUDependencyData &hpDependencyData);

	void handleEnterTaskwait(nosv_task_t task);
	void handleExitTaskwait(nosv_task_t task);

	void combineTaskReductions(nosv_task_t task, size_t cpuId);
	void translateReductionAddresses(nosv_task_t task, size_t cpuId,
		nanos6_address_translation_entry_t * translationTable, int totalSymbols);

	template <typename ProcessorType>
	inline bool processAllDataAccesses(nosv_task_t task, ProcessorType processor);

	//! \brief Mark a Taskwait fragment as completed
	//!
	//! \param[in] task is the Task that created the taskwait fragment
	//! \param[in] region is the taskwait region that has been completed
	//! \param[in] hpDependencyData is the CPUDependencyData used for delayed operations
	void releaseTaskwaitFragment(
		nosv_task_t task,
		DataAccessRegion region,
		size_t cpuId,
		CPUDependencyData &hpDependencyData);

	bool supportsDataTracking();

} // namespace DataAccessRegistration

#endif // DATA_ACCESS_REGISTRATION_HPP
