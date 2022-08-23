/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_METADATA_HPP
#define TASK_METADATA_HPP

#include <atomic>
#include <bitset>
#include <cassert>

#include <nosv.h>
#include <nosv/affinity.h>

#include "dependencies/discrete/TaskDataAccesses.hpp"

#define DATA_ALIGNMENT_SIZE sizeof(void *)


//! \brief Contains metadata of the task such as the args blocks
//! and other flags/attributes
class TaskMetadata {

public:

	enum {
		//! Flags added by the Mercurium compiler
		final_flag=0,
		if0_flag,
		taskloop_flag,
		//! Taskfors are no longer supported. Keep this flag
		//! because the compiler can still generate taskfors.
		//! We treat taskfors as normal tasks
		taskfor_flag,
		wait_flag,
		preallocated_args_block_flag,
		lint_verified_flag,
		taskiter_flag,
		taskiter_update_flag,
		//! Flags added by the NODES runtime. Note that
		//! these flags must be always declared after the
		//! Mercurium flags
		non_runnable_flag,
		spawned_flag,
		remote_flag,
		stream_executor_flag,
		main_task_flag,
		onready_completed_flag,
		total_flags
	};

private:

	typedef std::bitset<total_flags> flags_t;

	//! Args blocks of the task
	void *_argsBlock;

	//! The original size of args block
	size_t _argsBlockSize;

	//! Number of pending predecessors
	std::atomic<int> _predecessorCount;

	//! Number of children that are still alive +1 for dependencies
	std::atomic<int> _removalCount;

	//! Number of children that are still not finished, +1 if not blocked
	std::atomic<int> _countdownToBeWokenUp;

	//! Number of internal and external events that prevent the release of dependencies
	std::atomic<int> _countdownToRelease;

	//! Link to the parent task
	TaskMetadata *_parent;

	//! Whether the task has finished user code execution
	std::atomic<bool> _finished;

	//! Whether the task was executed inline if it is if0
	bool _if0Inlined;

	//! The size needed to allocate this metadata
	size_t _metadataSize;

	//! Whether the task's metadata was locally allocated (not allocd from nOS-V)
	bool _locallyAllocated;

	//! Special fields for tasks inside a taskiter
	//! Original precedessor count
	int _originalPredecessorCount;

	//! Iteration count
	size_t _iterationCount;

	//! Elapsed time
	uint64_t _elapsedTime;

protected:

	//! A pointer to the original task that wraps this metadata
	nosv_task_t _task;

	//! Dependencies of the task
	TaskDataAccesses _dataAccesses;

	//! Task flags
	flags_t _flags;

public:

	inline TaskMetadata(
		void *argsBlock,
		size_t argsBlockSize,
		nosv_task_t taskPointer,
		size_t flags,
		const TaskDataAccessesInfo &taskAccessInfo,
		size_t taskMetadataSize,
		bool locallyAllocated
	) :
		_argsBlock(argsBlock),
		_argsBlockSize(argsBlockSize),
		_predecessorCount(0),
		_removalCount(1),
		_countdownToBeWokenUp(1),
		_countdownToRelease(1),
		_parent(nullptr),
		_finished(false),
		_if0Inlined(true),
		_metadataSize(taskMetadataSize),
		_locallyAllocated(locallyAllocated),
		_originalPredecessorCount(-1),
		_iterationCount(0),
		_elapsedTime(0),
		_task(taskPointer),
		_dataAccesses(taskAccessInfo),
		_flags(flags)
	{
	}

	inline bool hasCode() const
	{
		nanos6_task_info_t *taskInfo = getTaskInfo(_task);
		assert(taskInfo->implementation_count == 1);
		assert(taskInfo != nullptr);

		return (taskInfo->implementations[0].run != nullptr);
	}

	inline void *getArgsBlock() const
	{
		return _argsBlock;
	}

	inline size_t getArgsBlockSize() const
	{
		return _argsBlockSize;
	}

	inline nosv_task_t getTaskHandle() const
	{
		return _task;
	}

	inline void increasePredecessors(int amount = 1)
	{
		_predecessorCount += amount;
	}

	//! \brief Decrease the number of predecessors
	//! \returns Whether the task becomes ready
	inline bool decreasePredecessors(int amount = 1)
	{
		int res = (_predecessorCount -= amount);
		assert(res >= 0);

		return (res == 0);
	}

	inline void increaseRemovalBlockingCount()
	{
		_removalCount.fetch_add(1, std::memory_order_relaxed);
	}

	//! \returns Whether the change makes this task become ready or disposable
	inline bool decreaseRemovalBlockingCount()
	{
		int countdown = (_removalCount.fetch_sub(1, std::memory_order_relaxed) - 1);
		assert(countdown >= 0);

		return (countdown == 0);
	}

	//! \brief Indicates if it does not have any children
	inline bool doesNotNeedToBlockForChildren() const
	{
		return (_removalCount == 1);
	}

	//! \brief Add a nested task
	inline void addChild()
	{
		_countdownToBeWokenUp.fetch_add(1, std::memory_order_relaxed);
		_removalCount.fetch_add(1, std::memory_order_relaxed);
	}

	inline void increaseWakeUpCount(int amount)
	{
		_countdownToBeWokenUp.fetch_add(amount, std::memory_order_relaxed);
	}

	inline int getRemovalCount()
	{
		return _removalCount.load();
	}

	//! \brief Remove a nested task (because it has finished)
	//! \return True if the change makes this task become ready
	inline bool finishChild() __attribute__((warn_unused_result))
	{
		int countdown = (_countdownToBeWokenUp.fetch_sub(1, std::memory_order_relaxed) - 1);
		assert(countdown >= 0);

		return (countdown == 0);
	}

	//! \brief Indicates if a task can be woken up
	inline bool canBeWokenUp() const
	{
		return (_countdownToBeWokenUp == 0);
	}

	//! \brief Mark the task as blocked
	//! \return True if the change makes the task become ready
	inline bool markAsBlocked()
	{
		int countdown = (_countdownToBeWokenUp.fetch_sub(1, std::memory_order_relaxed) - 1);
		assert(countdown >= 0);

		return (countdown == 0);
	}

	//! \brief Mark the task as unblocked
	//! \return True if it does not have any children
	inline bool markAsUnblocked()
	{
		return (_countdownToBeWokenUp.fetch_add(1, std::memory_order_relaxed) == 0);
	}

	//! \brief Reset the counter of events
	inline void resetReleaseCount()
	{
		assert(_countdownToRelease == 0);

		_countdownToRelease = 1;
	}

	//! \brief Increase the counter of events
	inline void increaseReleaseCount(int amount = 1)
	{
		assert(_countdownToRelease >= 0);

		_countdownToRelease += amount;
	}

	//! \brief Decrease the counter of events
	//!
	//! This function returns whether the decreased events were
	//! the last ones. This may mean that the task can start
	//! running if they were onready events or the task can release
	//! its dependencies if they were normal events
	//!
	//! \returns true iff were the last events
	inline bool decreaseReleaseCount(int amount = 1)
	{
		int count = (_countdownToRelease -= amount);
		assert(count >= 0);

		return (count == 0);
	}

	//! \brief Set the parent
	//! \param parent The actual parent of the task
	inline void setParent(nosv_task_t parent)
	{
		assert(parent != nullptr);

		// NOTE: It may happen that the task has no metadata (i.e. the attached task
		// from Initialization.cpp)
		// TODO: Assert that it can only be that task
		TaskMetadata **parentMetadataPointer = (TaskMetadata **) nosv_get_task_metadata(parent);
		if (parentMetadataPointer != nullptr) {
			TaskMetadata *parentMetadata = *parentMetadataPointer;
			assert(parentMetadata != nullptr);

			_parent = parentMetadata;
			parentMetadata->addChild();
		}
	}

	inline TaskMetadata *getParent() const
	{
		return _parent;
	}

	//! \brief Mark that the task has finished executing
	inline void markAsFinished()
	{
		_finished = true;
	}

	//! \brief Check whether the task has finished executing
	inline bool hasFinished() const
	{
		return _finished;
	}

	inline void markIf0AsNotInlined()
	{
		_if0Inlined = false;
	}

	inline bool isIf0Inlined() const
	{
		return _if0Inlined;
	}

	inline size_t getTaskMetadataSize() const
	{
		return _metadataSize;
	}

	inline bool isLocallyAllocated() const
	{
		return _locallyAllocated;
	}

	inline TaskDataAccesses &getTaskDataAccesses()
	{
		return _dataAccesses;
	}

	inline size_t getFlags() const
	{
		return _flags.to_ulong();
	}

	//! \brief Set or unset the if0 flag
	inline void setIf0(bool if0Value)
	{
		_flags[if0_flag] = if0Value;
	}

	//! \brief Check if the task is in if0 mode
	inline bool isIf0() const
	{
		return _flags[if0_flag];
	}

	//! \brief Set the wait behavior
	inline void setDelayedRelease(bool delayedReleaseValue)
	{
		_flags[wait_flag] = delayedReleaseValue;
	}

	//! \brief Check if the task has the wait clause
	inline bool mustDelayRelease() const
	{
		return _flags[wait_flag];
	}

	//! \brief Complete the delay of the dependency release
	inline void completeDelayedRelease()
	{
		assert(_flags[wait_flag]);

		_flags[wait_flag] = false;
	}

	inline bool hasPreallocatedArgsBlock() const
	{
		return _flags[preallocated_args_block_flag];
	}

	inline bool isSpawned() const
	{
		return _flags[spawned_flag];
	}

	inline void setSpawned(bool value = true)
	{
		_flags[spawned_flag] = value;
	}

	inline bool isFinal() const
	{
		return _flags[final_flag];
	}

	//! \brief Set or unset the taskloop flag
	inline void setTaskloop(bool taskloopValue)
	{
		_flags[taskloop_flag] = taskloopValue;
	}

	//! \brief Check if the task is a taskloop
	inline bool isTaskloop() const
	{
		return _flags[taskloop_flag];
	}

	virtual inline void increaseMaxChildDependencies()
	{
	}

	virtual inline bool isTaskloopSource() const
	{
		return false;
	}

	virtual inline void registerDependencies()
	{
		// Retreive the args block and taskinfo of the task
		nanos6_task_info_t *taskInfo = TaskMetadata::getTaskInfo(_task);
		assert(taskInfo != nullptr);

		taskInfo->register_depinfo(_argsBlock, nullptr, this);
	}

	static inline TaskMetadata *getTaskMetadata(nosv_task_t task)
	{
		// For info on the why of this double de-ref, check TaskCreation.cpp
		TaskMetadata **taskMetadataPointer = (TaskMetadata **) nosv_get_task_metadata(task);
		TaskMetadata *taskMetadata = nullptr;
		if (taskMetadataPointer != nullptr) {
			taskMetadata = *taskMetadataPointer;
			assert(taskMetadata != nullptr);
		}

		// TODO: Make sure that if we're returning nullptr it is due to the
		// wrapped initialization in (Initialization.hpp)
		return taskMetadata;
	}

	static inline TaskMetadata *getCurrentTask()
	{
		nosv_task_t task = nosv_self();
		return TaskMetadata::getTaskMetadata(task);
	}

	static inline nanos6_task_info_t *getTaskInfo(TaskMetadata *task)
	{
		nosv_task_t originalTask = task->getTaskHandle();
		nosv_task_type_t type = nosv_get_task_type(originalTask);
		return (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
	}

	static inline nanos6_task_info_t *getTaskInfo(nosv_task_t task)
	{
		nosv_task_type_t type = nosv_get_task_type(task);
		return (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
	}

	virtual inline bool isTaskiter() const
	{
		return false;
	}

	inline int getOriginalPrecessorCount() const
	{
		return _originalPredecessorCount;
	}

	inline void incrementOriginalPredecessorCount()
	{
		_originalPredecessorCount++;
	}

	inline void setIterationCount(size_t count)
	{
		_iterationCount = count;
	}

	inline bool decreaseIterations()
	{
		assert(_parent);
		assert(_parent->isTaskiter());
		return (--_iterationCount > 1);
	}

	inline void setPriority(int priority)
	{
		nosv_set_task_priority(getTaskHandle(), priority);
	}

	inline int getPriority() const
	{
		return nosv_get_task_priority(getTaskHandle());
	}

	inline void setElapsedTime(uint64_t elapsed)
	{
		_elapsedTime = elapsed;
	}

	inline uint64_t getElapsedTime() const
	{
		return _elapsedTime;
	}

	inline void setAffinity(uint32_t index, nosv_affinity_level_t level, nosv_affinity_type_t type)
	{
		nosv_affinity_t affinity = nosv_affinity_get(index, level, type);
		nosv_set_task_affinity(getTaskHandle(), &affinity);
	}

	virtual ~TaskMetadata() {}
};

#endif // TASK_METADATA_HPP
