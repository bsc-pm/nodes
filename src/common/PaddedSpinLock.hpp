/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef PADDED_SPIN_LOCK_HPP
#define PADDED_SPIN_LOCK_HPP

#include "Padding.hpp"
#include "SpinLock.hpp"


template <int PADDING = CACHELINE_SIZE>
class PaddedSpinLock {

private:

	char _frontPadding[PADDING - sizeof(SpinLock)];
	SpinLock _lock;
	char _backPadding[PADDING - sizeof(SpinLock)];

	template <int PADDING2>
	PaddedSpinLock operator=(const PaddedSpinLock<PADDING2> &) = delete;

	template <int PADDING2>
	PaddedSpinLock(const PaddedSpinLock<PADDING2> &) = delete;

public:

	inline PaddedSpinLock()
	{
	}

	inline void lock()
	{
		_lock.lock();
	}

	inline bool tryLock()
	{
		return _lock.tryLock();
	}

	inline void unlock()
	{
		_lock.unlock();
	}

	inline SpinLock &getSpinLock()
	{
		return _lock;
	}
};


#endif // PADDED_SPIN_LOCK_HPP
