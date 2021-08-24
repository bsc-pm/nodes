/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <mutex>

#include <nosv.h>

#include "BottomMapEntry.hpp"
#include "CommutativeSemaphore.hpp"
#include "CPUDependencyData.hpp"
#include "DataAccessRegistration.hpp"
#include "TaskDataAccesses.hpp"
#include "memory/ObjectAllocator.hpp"
#include "system/TaskCreation.hpp"
#include "system/TaskFinalization.hpp"


#define __unused __attribute__((unused))

#pragma GCC visibility push(hidden)


class ComputePlace;

namespace DataAccessRegistration {

	typedef TaskDataAccesses::bottom_map_t bottom_map_t;

	static inline void insertAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData);

	static inline ReductionInfo *allocateReductionInfo(
		DataAccessType &dataAccessType, reduction_index_t reductionIndex,
		reduction_type_and_operator_index_t reductionTypeAndOpIndex,
		void *address, const size_t length, const nosv_task &task);

	static inline void releaseReductionInfo(ReductionInfo *info);

	static inline void decreaseDeletableCountOrDelete(nosv_task_t originator,
		CPUDependencyData::deletable_originator_list_t &deletableOriginators);

	//! Process all the originators that have become ready
	static inline void processSatisfiedOriginators(CPUDependencyData &hpDependencyData)
	{
		for (int i = 0; i < nanos6_device_t::nanos6_device_type_num; ++i) {
			CPUDependencyData::satisfied_originator_list_t &list = hpDependencyData.getSatisfiedOriginators(i);
			if (list.size() > 0) {
				nosv_task_t *taskArray = list.getArray();
				for (size_t j = 0; j < list.size(); ++j) {
					nosv_submit(taskArray[j], NOSV_SUBMIT_UNLOCKED);
				}
			}
		}

		hpDependencyData.clearSatisfiedOriginators();

		for (nosv_task_t originator : hpDependencyData._satisfiedCommutativeOriginators) {
			nosv_submit(originator, NOSV_SUBMIT_UNLOCKED);
		}

		hpDependencyData._satisfiedCommutativeOriginators.clear();
	}

	static inline void processDeletableOriginators(CPUDependencyData &hpDependencyData)
	{
		// As there is no "task garbage collection", the runtime will only
		// destruct the tasks for us if we mark them as not needed on the
		// unregisterTaskDataAccesses call, so this takes care on tasks ended anywhere else

		for (nosv_task_t deletableOriginator : hpDependencyData._deletableOriginators) {
			assert(deletableOriginator != nullptr);

			TaskFinalization::disposeTask(deletableOriginator);
		}

		hpDependencyData._deletableOriginators.clear();
	}

	static inline void satisfyTask(nosv_task_t task, CPUDependencyData &hpDependencyData)
	{
		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		if (taskMetadata->decreasePredecessors()) {
			TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
			if (accessStruct._commutativeMask.any() && !CommutativeSemaphore::registerTask(task)) {
				return;
			}

			hpDependencyData.addSatisfiedOriginator(task);

			if (hpDependencyData.full()) {
				processSatisfiedOriginators(hpDependencyData);
			}
		}
	}

	static inline DataAccessType combineTypes(DataAccessType type1, DataAccessType type2)
	{
		if (type1 == type2) {
			return type1;
		}

		return READWRITE_ACCESS_TYPE;
	}

	static inline void upgradeAccess(DataAccess *access, DataAccessType newType, bool weak)
	{
		DataAccessType oldType = access->getType();

		// Let's not allow combining reductions with other types as it causes problems
		assert((oldType != REDUCTION_ACCESS_TYPE && newType != REDUCTION_ACCESS_TYPE) || (newType == oldType));

		access->setType(combineTypes(oldType, newType));

		// ! weak + weak = !weak :)
		if (access->isWeak() && !weak)
			access->setWeak(false);
	}

	void registerTaskDataAccess(
		nosv_task_t task, DataAccessType accessType, bool weak, void *address, size_t length,
		reduction_type_and_operator_index_t reductionTypeAndOperatorIndex,
		reduction_index_t reductionIndex, int symbolIndex)
	{
		// This is called once per access in the task and it's purpose is to initialize our DataAccess structure with the
		// arguments of this function. No dependency registration is done here, and this call precedes the "registerTaskDataAccesses"
		// one. All the access structs are constructed in-place in the task array, to prevent allocations.

		assert(task != nullptr);
		assert(address != nullptr);
		assert(length > 0);


		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		bool alreadyExisting;
		DataAccess *access = accessStruct.allocateAccess(address, accessType, task, length, weak, alreadyExisting);
		if (!alreadyExisting) {
			if (!weak) {
				accessStruct.incrementTotalDataSize(length);
			}

			if (accessType == REDUCTION_ACCESS_TYPE) {
				access->setReductionOperator(reductionTypeAndOperatorIndex);
				access->setReductionIndex(reductionIndex);
			}
		} else {
			upgradeAccess(access, accessType, weak);
		}

		access->addToSymbol(symbolIndex);
	}

	void propagateMessages(
		CPUDependencyData &hpDependencyData,
		mailbox_t &mailBox,
		ReductionInfo *originalReductionInfo)
	{
		DataAccessMessage next;

		while (!mailBox.empty()) {
			next = mailBox.top();
			mailBox.pop();

			assert(next.from != nullptr);

			if (next.to != nullptr && next.flagsForNext) {
				if (next.to->apply(next, mailBox)) {
					nosv_task_t task = next.to->getOriginator();
					TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
					assert(taskMetadata != nullptr);

					TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
					assert(!accessStruct.hasBeenDeleted());
					assert(next.to != next.from);

					decreaseDeletableCountOrDelete(task, hpDependencyData._deletableOriginators);
				}
			}

			bool dispose = false;

			if (next.schedule) {
				nosv_task_t task = next.from->getOriginator();
				TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
				assert(taskMetadata != nullptr);

				TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
				assert(!accessStruct.hasBeenDeleted());

				satisfyTask(task, hpDependencyData);
			}

			if (next.combine) {
				nosv_task_t task = next.from->getOriginator();
				TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
				assert(taskMetadata != nullptr);

				TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
				assert(!accessStruct.hasBeenDeleted());

				ReductionInfo *reductionInfo = next.from->getReductionInfo();
				assert(reductionInfo != nullptr);

				if (reductionInfo != originalReductionInfo) {
					if (reductionInfo->incrementUnregisteredAccesses()) {
						releaseReductionInfo(reductionInfo);
					}
					originalReductionInfo = reductionInfo;
				}
			}

			if (next.flagsAfterPropagation) {
				nosv_task_t task = next.from->getOriginator();
				TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
				assert(taskMetadata != nullptr);

				TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
				assert(!accessStruct.hasBeenDeleted());

				dispose = next.from->applyPropagated(next);
			}

			if (dispose) {
				nosv_task_t task = next.from->getOriginator();
				TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
				assert(taskMetadata != nullptr);

				TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
				assert(!accessStruct.hasBeenDeleted());

				decreaseDeletableCountOrDelete(task, hpDependencyData._deletableOriginators);
			}
		}
	}

	void finalizeDataAccess(nosv_task_t task, DataAccess *access, void *address, CPUDependencyData &hpDependencyData)
	{
		DataAccessType originalAccessType = access->getType();

		// No race, the parent is finished so all childs must be registered by now
		DataAccess *childAccess = access->getChild();
		ReductionInfo *reductionInfo = nullptr;

		mailbox_t &mailBox = hpDependencyData._mailBox;
		assert(mailBox.empty());

		access_flags_t flagsToSet = ACCESS_UNREGISTERED;

		if (childAccess == nullptr) {
			flagsToSet |= (ACCESS_CHILD_WRITE_DONE | ACCESS_CHILD_READ_DONE | ACCESS_CHILD_CONCURRENT_DONE | ACCESS_CHILD_COMMUTATIVE_DONE);
		} else {
			// Place ourselves as successors of the last access.
			DataAccess *lastChild = nullptr;

			TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
			assert(taskMetadata != nullptr);

			TaskDataAccesses &taskAccesses = taskMetadata->_dataAccesses;
			assert(!taskAccesses.hasBeenDeleted());

			bottom_map_t &bottomMap = taskAccesses._subaccessBottomMap;
			bottom_map_t::iterator itMap = bottomMap.find(address);
			assert(itMap != bottomMap.end());

			BottomMapEntry &node = itMap->second;
			lastChild = node._access;
			assert(lastChild != nullptr);

			lastChild->setSuccessor(access);
			DataAccessMessage m = lastChild->applySingle(ACCESS_HASNEXT | ACCESS_NEXTISPARENT, mailBox);
			__attribute__((unused)) DataAccessMessage m_debug = access->applySingle(m.flagsForNext, mailBox);
			assert(!(m_debug.flagsForNext));
			lastChild->applyPropagated(m);
		}

		if (originalAccessType == REDUCTION_ACCESS_TYPE) {
			reductionInfo = access->getReductionInfo();
			assert(reductionInfo != nullptr);

			if (reductionInfo->incrementUnregisteredAccesses()) {
				releaseReductionInfo(reductionInfo);
			}
		}

		// No need to worry here because no other thread can destroy this access, since we have to
		// finish unregistering all accesses before that can happen.
		DataAccessMessage message;
		message.to = message.from = access;
		message.flagsForNext = flagsToSet;
		bool dispose = access->apply(message, mailBox);

		if (!mailBox.empty()) {
			propagateMessages(hpDependencyData, mailBox, reductionInfo);
			assert(!dispose);
		} else if (dispose) {
			decreaseDeletableCountOrDelete(task, hpDependencyData._deletableOriginators);
		}
	}

	bool registerTaskDataAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData)
	{
		// This is called once per task, and will create all the dependencies in
		// register_depinfo, to later insert them into the chain in the insertAccesses call
		assert(task != nullptr);

#ifndef NDEBUG
		{
			bool alreadyTaken = false;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, true));
		}
#endif

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		// Increase the number of predecessors by two (avoiding races)
		taskMetadata->increasePredecessors(2);

		// Retreive the args block and taskinfo of the task
		nosv_task_type_t type = nosv_get_task_type(task);
		assert(type != nullptr);

		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);

		// This part creates the DataAccesses and inserts it to dependency system
		taskInfo->register_depinfo(taskMetadata->_argsBlock, nullptr, task);

		insertAccesses(task, hpDependencyData);

		TaskDataAccesses &accessStructures = taskMetadata->_dataAccesses;
		assert(!accessStructures.hasBeenDeleted());

		if (accessStructures.hasDataAccesses()) {
			taskMetadata->increaseRemovalBlockingCount();
		}

		processSatisfiedOriginators(hpDependencyData);
		processDeletableOriginators(hpDependencyData);

#ifndef NDEBUG
		{
			bool alreadyTaken = true;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, false));
		}
#endif

		bool ready = taskMetadata->decreasePredecessors(2);

		// Commutative accesses have to acquire the commutative region
		if (ready && accessStructures._commutativeMask.any()) {
			ready = CommutativeSemaphore::registerTask(task);
		}

		return ready;
	}

	void unregisterTaskDataAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData)
	{
		assert(task != nullptr);

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());
		assert(hpDependencyData._mailBox.empty());

#ifndef NDEBUG
		{
			bool alreadyTaken = false;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, true));
		}
#endif

		if (accessStruct.hasDataAccesses()) {
			// Release dependencies of all my accesses
			accessStruct.forAll([&](void *address, DataAccess *access) -> bool {
				// Skip if released
				if (!access->isReleased()) {
					finalizeDataAccess(task, access, address, hpDependencyData);
				}

				return true;
			});
		}

		bottom_map_t &bottomMap = accessStruct._subaccessBottomMap;

		for (bottom_map_t::iterator itMap = bottomMap.begin(); itMap != bottomMap.end(); itMap++) {
			DataAccess *access = itMap->second._access;
			assert(access != nullptr);

			DataAccessMessage m;
			m.from = m.to = access;
			m.flagsAfterPropagation = ACCESS_PARENT_DONE;
			if (access->applyPropagated(m)) {
				decreaseDeletableCountOrDelete(access->getOriginator(), hpDependencyData._deletableOriginators);
			}

			ReductionInfo *reductionInfo = itMap->second._reductionInfo;
			if (reductionInfo != nullptr) {
				// We cannot close this in case we had a weak reduction
				DataAccess *parentAccess = accessStruct.findAccess(itMap->first);

				if (parentAccess == nullptr || parentAccess->getType() != REDUCTION_ACCESS_TYPE) {
					assert(!reductionInfo->finished());
					if (reductionInfo->markAsClosed()) {
						releaseReductionInfo(reductionInfo);
					}

					itMap->second._reductionInfo = nullptr;
				} else {
					assert(parentAccess->isWeak());
				}
			}
		}

		// Release commutative mask. The order is important, as this will add satisfied originators
		if (accessStruct._commutativeMask.any())
			CommutativeSemaphore::releaseTask(task, hpDependencyData);

		if (accessStruct.hasDataAccesses()) {
			// All TaskDataAccesses have a deletableCount of 1 for default, so this will return true unless
			// some read/reduction accesses have increased this as well because the task cannot be deleted yet.
			// It also plays an important role in ensuring that a task will not be deleted by another one while
			// it's performing the dependency release

			if (accessStruct.decreaseDeletableCount()) {
				taskMetadata->decreaseRemovalBlockingCount();
			}
		}

		processSatisfiedOriginators(hpDependencyData);
		processDeletableOriginators(hpDependencyData);

#ifndef NDEBUG
		{
			bool alreadyTaken = true;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, false));
		}
#endif
	}

	void handleEnterTaskwait(nosv_task_t task)
	{
		assert(task != nullptr);

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		bottom_map_t &bottomMap = accessStruct._subaccessBottomMap;
		for (bottom_map_t::iterator itMap = bottomMap.begin(); itMap != bottomMap.end(); itMap++) {
			ReductionInfo *reductionInfo = itMap->second._reductionInfo;

			if (reductionInfo != nullptr) {
				DataAccess *parentAccess = accessStruct.findAccess(itMap->first);

				if (parentAccess == nullptr || parentAccess->getType() != REDUCTION_ACCESS_TYPE) {
					assert(!reductionInfo->finished());
					if (reductionInfo->markAsClosed())
						releaseReductionInfo(reductionInfo);

					itMap->second._reductionInfo = nullptr;
				} else {
					assert(parentAccess->isWeak());
				}
			}
		}
	}

	void handleExitTaskwait(nosv_task_t)
	{
	}

	static inline void insertAccesses(nosv_task_t task, CPUDependencyData &hpDependencyData)
	{
		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		nosv_task_t parentTask = taskMetadata->_parent;
		TaskMetadata *parentTaskMetadata = (TaskMetadata *) nosv_get_task_metadata(parentTask);
		assert(parentTaskMetadata != nullptr);

		TaskDataAccesses &parentAccessStruct = parentTaskMetadata->_dataAccesses;
		assert(!parentAccessStruct.hasBeenDeleted());

		mailbox_t &mailBox = hpDependencyData._mailBox;
		assert(mailBox.empty());

		// Default deletableCount of 1
		accessStruct.increaseDeletableCount();

		// Get all seqs
		accessStruct.forAll([&](void *address, DataAccess *access) -> bool {
			DataAccessType accessType = access->getType();
			ReductionInfo *reductionInfo = nullptr;
			DataAccess *predecessor = nullptr;
			bottom_map_t::iterator itMap;
			bool weak = access->isWeak();

			accessStruct.increaseDeletableCount();

			bottom_map_t &addresses = parentAccessStruct._subaccessBottomMap;
			// Determine our predecessor safely, and maybe insert ourselves to the map.
			std::pair<bottom_map_t::iterator, bool> result = addresses.emplace(std::piecewise_construct,
				std::forward_as_tuple(address),
				std::forward_as_tuple(access));

			itMap = result.first;

			if (!result.second) {
				// Element already exists.
				predecessor = itMap->second._access;
				itMap->second._access = access;
			}

			if (accessType == COMMUTATIVE_ACCESS_TYPE && !weak) {
				// Calculate commutative mask
				CommutativeSemaphore::combineMaskAndAddress(accessStruct._commutativeMask, address);
			}

			bool dispose = false;
			bool schedule = false;
			DataAccessMessage fromCurrent;
			DataAccess *parentAccess = nullptr;

			if (predecessor == nullptr) {
				parentAccess = parentAccessStruct.findAccess(address);

				if (parentAccess != nullptr) {
					// In case we need to inherit reduction
					reductionInfo = parentAccess->getReductionInfo();
					// Check that if we got something the parent is weakreduction
					assert(reductionInfo == nullptr || parentAccess->isWeak());
				}
			}

			// Check if we're closing a reduction, or allocate one in case we need it.
			if (accessType == REDUCTION_ACCESS_TYPE) {
				// Get the reduction info from the bottom map. If there is none, check
				// if our parent has one (for weak reductions)
				ReductionInfo *currentReductionInfo = itMap->second._reductionInfo;
				reduction_type_and_operator_index_t typeAndOpIndex = access->getReductionOperator();
				size_t length = access->getLength();

				if (currentReductionInfo == nullptr) {
					currentReductionInfo = reductionInfo;
					// Inherited reductions must be equal
					assert(reductionInfo == nullptr || (reductionInfo->getTypeAndOperatorIndex() == typeAndOpIndex && reductionInfo->getOriginalLength() == length));
				} else {
					reductionInfo = currentReductionInfo;
				}

				if (currentReductionInfo == nullptr || currentReductionInfo->getTypeAndOperatorIndex() != typeAndOpIndex || currentReductionInfo->getOriginalLength() != length) {
					currentReductionInfo = allocateReductionInfo(accessType, access->getReductionIndex(), typeAndOpIndex,
						address, length, *task);
				}

				currentReductionInfo->incrementRegisteredAccesses();
				itMap->second._reductionInfo = currentReductionInfo;

				assert(currentReductionInfo != nullptr);
				assert(currentReductionInfo->getTypeAndOperatorIndex() == typeAndOpIndex);
				assert(currentReductionInfo->getOriginalLength() == length);
				assert(currentReductionInfo->getOriginalAddress() == address);

				access->setReductionInfo(currentReductionInfo);
			} else {
				reductionInfo = itMap->second._reductionInfo;
				itMap->second._reductionInfo = nullptr;
			}

			if (predecessor == nullptr) {
				if (parentAccess != nullptr) {
					parentAccess->setChild(access);

					DataAccessMessage message = parentAccess->applySingle(ACCESS_HASCHILD, mailBox);
					fromCurrent = access->applySingle(message.flagsForNext, mailBox);
					schedule = fromCurrent.schedule;
					assert(!(fromCurrent.flagsForNext));

					dispose = parentAccess->applyPropagated(message);
					assert(!dispose);

					if (dispose)
						decreaseDeletableCountOrDelete(parentTask, hpDependencyData._deletableOriginators);
				} else {
					schedule = true;
					fromCurrent = access->applySingle(
						ACCESS_READ_SATISFIED | ACCESS_WRITE_SATISFIED | ACCESS_CONCURRENT_SATISFIED | ACCESS_COMMUTATIVE_SATISFIED,
						mailBox);
				}
			} else {
				predecessor->setSuccessor(access);

				DataAccessMessage message = predecessor->applySingle(ACCESS_HASNEXT, mailBox);
				fromCurrent = access->applySingle(message.flagsForNext, mailBox);
				schedule = fromCurrent.schedule;
				assert(!(fromCurrent.flagsForNext));

				dispose = predecessor->applyPropagated(message);
				if (dispose)
					decreaseDeletableCountOrDelete(predecessor->getOriginator(), hpDependencyData._deletableOriginators);
			}

			if (fromCurrent.combine) {
				assert(access->getType() == REDUCTION_ACCESS_TYPE);
				assert(fromCurrent.flagsAfterPropagation == ACCESS_REDUCTION_COMBINED);
				ReductionInfo *current = access->getReductionInfo();
				if (current != reductionInfo) {
					dispose = current->incrementUnregisteredAccesses();
					assert(!dispose);
				}

				dispose = access->applyPropagated(fromCurrent);
				assert(!dispose);
			}

			if (reductionInfo != nullptr && access->getReductionInfo() != reductionInfo) {
				if (reductionInfo->markAsClosed())
					releaseReductionInfo(reductionInfo);
			}

			// Weaks and reductions always start
			if (accessType == REDUCTION_ACCESS_TYPE || weak)
				schedule = true;

			if (!schedule) {
				taskMetadata->increasePredecessors();
			}

			return true; // Continue iteration
		});
	}

	static inline void releaseReductionInfo(ReductionInfo *info)
	{
		assert(info != nullptr);
		assert(info->finished());

		info->combine();

		ObjectAllocator<ReductionInfo>::deleteObject(info);
	}

	static inline void decreaseDeletableCountOrDelete(nosv_task_t originator,
		CPUDependencyData::deletable_originator_list_t &deletableOriginators)
	{
		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(originator);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		if (accessStruct.decreaseDeletableCount()) {
			if (taskMetadata->decreaseRemovalBlockingCount()) {
				deletableOriginators.push_back(originator); // Ensure destructor is called
			}
		}
	}

	static inline ReductionInfo *allocateReductionInfo(
		__unused DataAccessType &dataAccessType, reduction_index_t reductionIndex,
		reduction_type_and_operator_index_t reductionTypeAndOpIndex,
		void *address, const size_t length, const nosv_task &task)
	{
		assert(dataAccessType == REDUCTION_ACCESS_TYPE);

		// Retreive the args block and taskinfo of the task
		nosv_task_type_t type = nosv_get_task_type((nosv_task_t) &task);
		assert(type != nullptr);

		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);

		ReductionInfo *newReductionInfo = ObjectAllocator<ReductionInfo>::newObject(
			address, length,
			reductionTypeAndOpIndex,
			taskInfo->reduction_initializers[reductionIndex],
			taskInfo->reduction_combiners[reductionIndex]);

		return newReductionInfo;
	}

	void combineTaskReductions(nosv_task_t task, size_t cpuId)
	{
		assert(task != nullptr);

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		if (!accessStruct.hasDataAccesses())
			return;

		accessStruct.forAll([&](void *, DataAccess *access) -> bool {
			// Skip if released
			if (access->isReleased())
				return true;

			if (access->getType() == REDUCTION_ACCESS_TYPE && !access->isWeak()) {
				ReductionInfo *reductionInfo = access->getReductionInfo();
				reductionInfo->releaseSlotsInUse(task, cpuId);
			}

			return true;
		});
	}

	void translateReductionAddresses(nosv_task_t task, size_t cpuId,
		nanos6_address_translation_entry_t *translationTable,
		int totalSymbols)
	{
		assert(task != nullptr);
		assert(translationTable != nullptr);

		// Initialize translationTable
		for (int i = 0; i < totalSymbols; ++i)
			translationTable[i] = {0, 0};

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

		accessStruct.forAll([&](void *address, DataAccess *access) {
			if (access->getType() == REDUCTION_ACCESS_TYPE && !access->isWeak()) {
				ReductionInfo *reductionInfo = access->getReductionInfo();
				assert(reductionInfo != nullptr);

				void *translation = reductionInfo->getFreeSlot(task, cpuId);

				for (int j = 0; j < totalSymbols; ++j) {
					if (access->isInSymbol(j)) {
						translationTable[j] = {(size_t)address, (size_t)translation};
					}
				}
			}

			return true; // Continue iteration
		});
	}

	void releaseAccessRegion(
		nosv_task_t task,
		void *address,
		DataAccessType accessType,
		bool weak,
		size_t cpuId,
		CPUDependencyData &hpDependencyData)
	{
		assert(task != nullptr);
		assert(hpDependencyData._mailBox.empty());

		TaskMetadata *taskMetadata = (TaskMetadata *) nosv_get_task_metadata(task);
		assert(taskMetadata != nullptr);

		TaskDataAccesses &accessStruct = taskMetadata->_dataAccesses;
		assert(!accessStruct.hasBeenDeleted());

#ifndef NDEBUG
		{
			bool alreadyTaken = false;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, true));
		}
#endif

		if (accessStruct.hasDataAccesses()) {
			// Release dependencies of all my accesses
			DataAccess *access = accessStruct.findAccess(address);

			// Some unlikely sanity checks
			ErrorHandler::failIf(access == nullptr,
				"Attempt to release an access that was not originally registered in the task");

			ErrorHandler::failIf(access->getType() != accessType || access->isWeak() != weak,
				"It is not possible to partially release a dependence.");

			// Release reduction storage before finalizing, as we might delete the ReductionInfo later
			if (access->getType() == REDUCTION_ACCESS_TYPE && !access->isWeak()) {
				ReductionInfo *reductionInfo = access->getReductionInfo();
				assert(reductionInfo != nullptr);

				reductionInfo->releaseSlotsInUse(task, cpuId);
			}

			finalizeDataAccess(task, access, address, hpDependencyData);
		} else {
			ErrorHandler::fail("Attempt to release an access that was not originally registered in the task");
		}

		// Unfortunately, due to the CommutativeSemaphore implementation, we cannot release the commutative mask.
		// This is because it can be aliased between accesses, although if a counter was added for the number of
		// commutative accesses, it would be possible to find out how safe is it to release the mask.
		processSatisfiedOriginators(hpDependencyData);
		processDeletableOriginators(hpDependencyData);

#ifndef NDEBUG
		{
			bool alreadyTaken = true;
			assert(hpDependencyData._inUse.compare_exchange_strong(alreadyTaken, false));
		}
#endif
	}

	void releaseTaskwaitFragment(
		__attribute__((unused)) nosv_task_t task,
		__attribute__((unused)) DataAccessRegion region,
		__attribute__((unused)) size_t cpuId,
		__attribute__((unused)) CPUDependencyData &hpDependencyData)
	{
		assert(false);
	}

	bool supportsDataTracking()
	{
		return true;
	}

} // namespace DataAccessRegistration

#pragma GCC visibility pop
