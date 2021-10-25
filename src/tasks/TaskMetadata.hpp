/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_METADATA_HPP
#define TASK_METADATA_HPP

#include <atomic>
#include <bitset>
#include <cassert>

#include <nosv.h>

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
		taskfor_flag,
		wait_flag,
		preallocated_args_block_flag,
		lint_verified_flag,
		//! Flags added by the Nanos6 runtime. Note that
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
	nosv_task_t _parent;

	//! Whether the task has finished user code execution
	std::atomic<bool> _finished;

	//! Whether the task was executed inline if it is if0
	bool _if0Inlined;

	//! The size needed to allocate this metadata
	size_t _metadataSize;

	//! Whether the task's metadata was locally allocated (not allocd from nOS-V)
	bool _locallyAllocated;

protected:

	//! Dependencies of the task
	TaskDataAccesses _dataAccesses;

	//! Task flags
	flags_t _flags;

public:

	inline TaskMetadata(
		void *argsBlock,
		size_t argsBlockSize,
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
		_dataAccesses(taskAccessInfo),
		_flags(flags),
		_metadataSize(taskMetadataSize),
		_locallyAllocated(locallyAllocated)
	{
	}

	inline void *getArgsBlock() const
	{
		return _argsBlock;
	}

	inline size_t getArgsBlockSize() const
	{
		return _argsBlockSize;
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
		assert(_countdownToRelease > 0);

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

		_parent = parent;

		// Retreive the task's metadata
		TaskMetadata *parentMetadata = getTaskMetadata(parent);
		parentMetadata->addChild();
	}

	inline nosv_task_t getParent() const
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

	virtual inline void registerDependencies(nosv_task_t task)
	{
		// Retreive the args block and taskinfo of the task
		nosv_task_type_t type = nosv_get_task_type(task);
		nanos6_task_info_t *taskInfo = (nanos6_task_info_t *) nosv_get_task_type_metadata(type);
		assert(taskInfo != nullptr);

		taskInfo->register_depinfo(_argsBlock, nullptr, task);
	}

	static inline TaskMetadata *getTaskMetadata(nosv_task_t task)
	{
		// For info on why, TaskCreation.cpp
		TaskMetadata **taskMetadataPointer = (TaskMetadata **) nosv_get_task_metadata(task);
		assert(taskMetadataPointer != nullptr);

		TaskMetadata *taskMetadata = *taskMetadataPointer;
		assert(taskMetadata != nullptr);

		return taskMetadata;
	}

};

#endif // TASK_METADATA_HPP
