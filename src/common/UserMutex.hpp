/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef USER_MUTEX_HPP
#define USER_MUTEX_HPP

#include <atomic>
#include <cassert>
#include <deque>
#include <mutex>

#include <nosv.h>

#include "SpinLock.hpp"
#include "SpinWait.hpp"


class UserMutex {

	//! \brief The user mutex state
	std::atomic<bool> _userMutex;

	//! \brief Lock for the queue of blocked tasks on this user-side mutex
	SpinLock _blockedTasksLock;

	//! \brief The list of tasks blocked on this user-side mutex
	std::deque<nosv_task_t> _blockedTasks;

public:

	//! \brief Initialize the mutex
	//! \param[in] initialState true if the mutex must be initialized in the locked state
	inline UserMutex(bool initialState) :
		_userMutex(initialState),
		_blockedTasksLock(),
		_blockedTasks()
	{
	}

	//! \brief Try to lock
	//! \returns Whether the user-lock has been locked successful
	inline bool tryLock()
	{
		bool expected = false;
		bool successful = _userMutex.compare_exchange_strong(expected, true);
		assert(expected != successful);

		return successful;
	}

	//! \brief Grab the lock and spin instead of blocking the task
	inline void spinLock()
	{
		bool expected = false;
		while (!_userMutex.compare_exchange_weak(expected, true)) {
			do {
				spinWait();
			} while (_userMutex.load(std::memory_order_relaxed));

			spinWaitRelease();
			expected = false;
		}
	}

	//! \brief Try to lock or queue the task
	//! \param[in] task The task that will be queued if the lock cannot be acquired
	//! \returns Whether the lock has been acquired (if false, the task has been queued)
	inline bool lockOrQueue(nosv_task_t task)
	{
		std::lock_guard<SpinLock> guard(_blockedTasksLock);
		if (tryLock()) {
			return true;
		} else {
			_blockedTasks.push_back(task);
			return false;
		}
	}

	inline nosv_task_t dequeueOrUnlock()
	{
		std::lock_guard<SpinLock> guard(_blockedTasksLock);

		if (_blockedTasks.empty()) {
			_userMutex = false;
			return nullptr;
		}

		nosv_task_t releasedTask = _blockedTasks.front();
		_blockedTasks.pop_front();
		assert(releasedTask != nullptr);

		return releasedTask;
	}
};

#endif // USER_MUTEX_HPP
