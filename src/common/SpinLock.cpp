/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include "SpinLock.hpp"
#include "SpinWait.hpp"

#ifndef SPIN_LOCK_READS_BETWEEN_CMPXCHG
#define SPIN_LOCK_READS_BETWEEN_CMPXCHG 1000
#endif


void SpinLock::lock()
{
	bool expected = false;
	while (!_lock.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
		int spinsLeft = SPIN_LOCK_READS_BETWEEN_CMPXCHG;
		do {
			spinWait();
			spinsLeft--;
		} while (_lock.load(std::memory_order_relaxed) && (spinsLeft > 0));

		spinWaitRelease();

		expected = false;
	}
}

bool SpinLock::tryLock()
{
	bool expected = false;
	bool success = _lock.compare_exchange_strong(expected, true, std::memory_order_acquire);
	return success;
}

void SpinLock::unlock()
{
	_lock.store(false, std::memory_order_release);
}
