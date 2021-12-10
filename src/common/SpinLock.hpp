/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef SPIN_LOCK_HPP
#define SPIN_LOCK_HPP

#include <atomic>


class SpinLock {

private:

	SpinLock operator=(const SpinLock &) = delete;
	SpinLock(const SpinLock &) = delete;

	std::atomic<bool> _lock;

public:

	inline SpinLock()
		: _lock(0)
	{
	}

	inline ~SpinLock()
	{
	}

	void lock();

	bool tryLock();

	void unlock();
};

#endif // SPIN_LOCK_HPP
