/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2024 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nodes/coroutines.hpp>

#include "common/Chrono.hpp"
#include "common/UserMutex.hpp"
#include "dependencies/discrete/DataAccessRegistration.hpp"
#include "instrument/OVNIInstrumentation.hpp"
#include "tasks/TaskMetadata.hpp"


bool oss_taskwait_awaitable::await_ready()
{
	Instrument::enterTaskWait();

	// Retreive the task's metadata
	TaskMetadata *taskMetadata = TaskMetadata::getCurrentTask();
	assert(taskMetadata != nullptr);

	if (taskMetadata->doesNotNeedToBlockForChildren()) {
		std::atomic_thread_fence(std::memory_order_acquire);
		Instrument::exitTaskWait();

		// No resume
		return true;
	}

	DataAccessRegistration::handleEnterTaskwait(taskMetadata);
	bool done = taskMetadata->markAsBlocked();

	this->need_resume = true;

	Instrument::exitTaskWait();
	if (!done) {
		// Suspend and resume
		return false;
	}

	// Resume
	return true;
}

void oss_taskwait_awaitable::await_suspend(std::coroutine_handle<> h)
{
	// Nothing, other tasks must resubmit
	nosv_set_suspend_mode(NOSV_SUSPEND_MODE_NONE, 0);
}

void oss_taskwait_awaitable::await_resume()
{
	if (this->need_resume) {
		Instrument::enterTaskWait();
		std::atomic_thread_fence(std::memory_order_acquire);

		TaskMetadata *taskMetadata = TaskMetadata::getCurrentTask();
		assert(taskMetadata != nullptr);
		assert(taskMetadata->canBeWokenUp());

		taskMetadata->markAsUnblocked();

		DataAccessRegistration::handleExitTaskwait(taskMetadata);

		Instrument::exitTaskWait();
	}
}

oss_taskwait_awaitable oss_co_taskwait()
{
	return oss_taskwait_awaitable{false};
}

std::suspend_always oss_co_suspend_current_task()
{
	return std::suspend_always{};
}

bool oss_waitfor_awaitable::await_ready()
{
	return false;
}

void oss_waitfor_awaitable::await_suspend(std::coroutine_handle<> h)
{
	this->startTime = Chrono::now<uint64_t>();

	// Convert timeout to nanoseconds (NODES API is in microseconds, nOS-V API is in nanoseconds)
	nosv_set_suspend_mode(NOSV_SUSPEND_MODE_TIMEOUT_SUBMIT, timeout * 1000);
}

void oss_waitfor_awaitable::await_resume()
{
	if (this->startTime != 0) {
		uint64_t finalTime = Chrono::now<uint64_t>();
		*(this->waitTime) = (finalTime - startTime);
	} else {
		*(this->waitTime) = 0;
	}
}

oss_waitfor_awaitable oss_co_wait_for(uint64_t timeout, uint64_t *waitTime)
{
	return oss_waitfor_awaitable{timeout, 0, waitTime};
}

bool oss_user_lock_awaitable::await_ready()
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
			return true;
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
		return true;
	}

	TaskMetadata *currentTask = TaskMetadata::getCurrentTask();
	assert(currentTask != nullptr);

	// Acquire the lock if possible. Otherwise queue the task
	if (userMutex->lockOrQueue(currentTask)) {
		return true;
	}

	return false;
}

void oss_user_lock_awaitable::await_suspend(std::coroutine_handle<> h)
{
	nosv_set_suspend_mode(NOSV_SUSPEND_MODE_NONE, 0);
}

void oss_user_lock_awaitable::await_resume()
{
	// This in combination with a release from other threads makes their changes visible to this one
	std::atomic_thread_fence(std::memory_order_acquire);
}

oss_user_lock_awaitable oss_co_user_lock(void **handlerPointer)
{
	return oss_user_lock_awaitable{handlerPointer};
}

bool oss_yield_awaitable::await_ready()
{
	return false;
}

void oss_yield_awaitable::await_suspend(std::coroutine_handle<> h)
{
	nosv_set_suspend_mode(NOSV_SUSPEND_MODE_SUBMIT, 1);
}

void oss_yield_awaitable::await_resume()
{
}

oss_yield_awaitable oss_co_yield(void)
{
	return oss_yield_awaitable{};
}
