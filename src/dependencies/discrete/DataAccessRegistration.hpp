/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_REGISTRATION_HPP
#define DATA_ACCESS_REGISTRATION_HPP

#include <cstddef>

#include <nodes/task-instantiation.h>

#include "CPUDependencyData.hpp"
#include "DataAccess.hpp"
#include "ReductionSpecific.hpp"
#include "dependencies/DataAccessType.hpp"
#include "tasks/TaskMetadata.hpp"


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
		TaskMetadata *task, DataAccessType accessType, bool weak, void *address, size_t length,
		reduction_type_and_operator_index_t reductionTypeAndOperatorIndex, reduction_index_t reductionIndex, int symbolIndex);

	//! \brief Performs the task dependency registration procedure
	//!
	//! \param[in] task the Task whose dependencies need to be calculated
	//!
	//! \returns true if the task is already ready
	bool registerTaskDataAccesses(TaskMetadata *task, CPUDependencyData &hpDependencyData);

	bool unregisterTaskDataAccesses(TaskMetadata *task, CPUDependencyData &hpDependencyData, bool fromBusyThread = false);

	void releaseAccessRegion(
		TaskMetadata *task, void * address,
		DataAccessType accessType,
		bool weak,
		size_t cpuId,
		CPUDependencyData &hpDependencyData);

	void handleEnterTaskwait(TaskMetadata *task);
	void handleExitTaskwait(TaskMetadata *task);

	void combineTaskReductions(TaskMetadata *task, size_t cpuId);
	void translateReductionAddresses(TaskMetadata *task, size_t cpuId,
		nanos6_address_translation_entry_t * translationTable, int totalSymbols);

	template <typename ProcessorType>
	inline bool processAllDataAccesses(TaskMetadata *task, ProcessorType processor);

	//! \brief Mark a Taskwait fragment as completed
	//!
	//! \param[in] task is the Task that created the taskwait fragment
	//! \param[in] region is the taskwait region that has been completed
	//! \param[in] hpDependencyData is the CPUDependencyData used for delayed operations
	void releaseTaskwaitFragment(
		TaskMetadata *task,
		DataAccessRegion region,
		size_t cpuId,
		CPUDependencyData &hpDependencyData);

	bool supportsDataTracking();

} // namespace DataAccessRegistration

#endif // DATA_ACCESS_REGISTRATION_HPP
