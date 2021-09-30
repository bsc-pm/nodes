/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <atomic>
#include <cassert>

#include <nosv.h>

#include <api/user-mutex.h>

#include "TaskBlocking.hpp"
#include "common/UserMutex.hpp"


typedef std::atomic<UserMutex *> mutex_t;

void nanos6_user_lock(void **handlerPointer, char const *)
{
	assert(handlerPointer != nullptr);

	// Allocation
	mutex_t &userMutexReference = (mutex_t &) *handlerPointer;
	if (__builtin_expect(userMutexReference == nullptr, 0)) {
		UserMutex *newMutex = new UserMutex(true);
		UserMutex *expected = nullptr;
		if (userMutexReference.compare_exchange_strong(expected, newMutex)) {
			// Successfully assigned new mutex
			assert(userMutexReference == newMutex);

			// Since we allocate the mutex in the locked state, the thread already owns it and the work is done
			return;
		} else {
			// Another thread managed to initialize it before us
			assert(expected != nullptr);
			assert(userMutexReference == expected);

			delete newMutex;
			// Continue through the "normal" path
		}
	}

	// The mutex has already been allocated and cannot change, so skip the atomic part from now on
	UserMutex *userMutex = userMutexReference.load();
	if (userMutex->tryLock()) {
		return;
	}

	nosv_task_t currentTask = nosv_self();
	assert(currentTask != nullptr);

	// Acquire the lock if possible. Otherwise queue the task
	if (userMutex->lockOrQueue(currentTask)) {
		return;
	}

	// Block the task
	nosv_pause(NOSV_SUBMIT_NONE);

	// This in combination with a release from other threads makes their changes visible to this one
	std::atomic_thread_fence(std::memory_order_acquire);
}

void nanos6_user_unlock(void **handlerPointer)
{
	assert(handlerPointer != nullptr);
	assert(*handlerPointer != nullptr);

	// This in combination with an acquire from another thread makes the changes visible to that one
	std::atomic_thread_fence(std::memory_order_release);

	mutex_t &userMutexReference = (mutex_t &) *handlerPointer;
	UserMutex &userMutex = *(userMutexReference.load());
	Task *releasedTask = userMutex.dequeueOrUnlock();
	if (releasedTask != nullptr) {
		nosv_submit(releasedTask, NOSV_SUBMIT_UNLOCKED);
	}
}
