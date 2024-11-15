 /*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2024 Barcelona Supercomputing Center (BSC)
*/

#ifndef NODES_COROUTINES_HPP
#define NODES_COROUTINES_HPP

#include <coroutine>
#include <cstddef>
#include <cstdint>

#include "major.h"

#pragma GCC visibility push(default)

struct oss_coroutine {
	struct promise_type {
		void *operator new(size_t size);
		void operator delete(void *ptr);
		oss_coroutine get_return_object() { return oss_coroutine{std::coroutine_handle<promise_type>::from_promise(*this)}; }
		std::suspend_never initial_suspend() { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }
		void return_void() {}
		void unhandled_exception() {}
	};
	oss_coroutine(std::coroutine_handle<> h) { handle = h; }
	std::coroutine_handle<> handle = nullptr;
};

struct oss_taskwait_awaitable {
	bool need_resume;

	bool await_ready();
	void await_suspend(std::coroutine_handle<> h);
	void await_resume();
};

struct oss_waitfor_awaitable {
	uint64_t timeout;
	uint64_t startTime;
	uint64_t *waitTime;

	bool await_ready();
	void await_suspend(std::coroutine_handle<> h);
	void await_resume();
};

struct oss_user_lock_awaitable {
	void **handlerPointer;

	bool await_ready();
	void await_suspend(std::coroutine_handle<> h);
	void await_resume();
};

struct oss_yield_awaitable {
	bool await_ready();
	void await_suspend(std::coroutine_handle<> h);
	void await_resume();
};

//! \brief Awaitable version of nanos6_taskwait
//! Block the control flow of the current task until all of its children have finished
oss_taskwait_awaitable oss_co_taskwait();

//! \brief Awaitable version of nanos6_block_current_task
//!
//! This function blocks the execution of the current task, and the runtime may choose
//! to execute other tasks within the execution scope of this call
std::suspend_always oss_co_suspend_current_task();

//! \brief Awaitable version of nanos6_user_lock
//!
//! Performs a lock over a mutex (of type void *) that must be initially
//! initialized to nullptr. The first call to this function performs the actual
//! mutex allocation and stores the handler in the address that is passed
//!
//! \param[in,out] handlerPointer a pointer to the handler, which is of type void *, that represent the mutex
oss_user_lock_awaitable oss_co_user_lock(void **handlerPointer);

//! \brief Awaitable version of nanos6_wait_for
//!
//! Pause the current task for approximately the amount of microseconds
//! passed as a parameter. The runtime may choose to execute other
//! tasks within the execution scope of this call
//!
//! \param[in] timeout the time that should be spent while paused in microseconds
//! \param[out] waitTime the actual time spent during the pause in microseconds
oss_waitfor_awaitable oss_co_wait_for(uint64_t timeout, uint64_t *waitTime);

//! \brief Awaitable version of nanos6_yield
//!
//! The task is paused and resubmitted again. The runtime may choose to
//! execute other tasks within the execution scope of this call
oss_yield_awaitable oss_co_yield();

#pragma GCC visibility pop

#endif /* NODES_COROUTINES_HPP */
