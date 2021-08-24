/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_CREATION_HPP
#define TASK_CREATION_HPP

#include <atomic>
#include <bitset>
#include <cassert>

#include <nosv.h>

#include "dependencies/discrete/TaskDataAccesses.hpp"

#define DATA_ALIGNMENT_SIZE sizeof(void *)


//! \brief Contains metadata of the task such as the args blocks
//! and other flags/attributes
struct TaskMetadata {

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

	typedef std::bitset<total_flags> flags_t;

	//! Args blocks of the task
	void *_argsBlock;

	//! Dependencies of the task
	TaskDataAccesses _dataAccesses;

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

	//! Task flags
	flags_t _flags;


	//    METHODS    //

	//! \brief Increase the number of predecessors
	inline void increasePredecessors(int amount = 1)
	{
		_predecessorCount += amount;
	}

	//! \brief Decrease the number of predecessors
	//! \returns true if the task becomes ready
	inline bool decreasePredecessors(int amount = 1)
	{
		int res = (_predecessorCount -= amount);
		assert(res >= 0);

		return (res == 0);
	}

	//! \brief Increase an internal counter to prevent the removal of the task
	inline void increaseRemovalBlockingCount()
	{
		_removalCount.fetch_add(1, std::memory_order_relaxed);
	}

	//! \brief Decrease an internal counter that prevents the removal of the task
	//!
	//! \returns True if the change makes this task become ready or disposable
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

	//! \brief Set the parent
	//! \param parent The actual parent of the task
	inline void setParent(nosv_task_t parent)
	{
		assert(parent != nullptr);

		_parent = parent;

		// Retreive the task's metadata
		TaskMetadata *parentMetadata = (TaskMetadata *) nosv_get_task_metadata(parent);
		assert(parentMetadata != nullptr);

		parentMetadata->addChild();
	}

	//! \brief Add a nested task
	inline void addChild()
	{
		_countdownToBeWokenUp.fetch_add(1, std::memory_order_relaxed);
		_removalCount.fetch_add(1, std::memory_order_relaxed);
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

};

#endif // TASK_CREATION_HPP
