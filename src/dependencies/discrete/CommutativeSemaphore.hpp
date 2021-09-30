/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef COMMUTATIVE_SEMAPHORE_HPP
#define COMMUTATIVE_SEMAPHORE_HPP

#include <bitset>
#include <cstdint>

#include <nosv.h>

#include "common/Containers.hpp"
#include "common/PaddedTicketSpinLock.hpp"


struct CPUDependencyData;

class CommutativeSemaphore {

	static constexpr int commutative_mask_bits = CACHELINE_SIZE * 8;

public:

	typedef std::bitset<commutative_mask_bits> commutative_mask_t;

	static bool registerTask(nosv_task_t task);

	static void releaseTask(nosv_task_t task, CPUDependencyData &hpDependencyData);

	static inline void combineMaskAndAddress(commutative_mask_t &mask, void *address)
	{
		mask.set(addressHash(address) % commutative_mask_bits);
	}

private:

	typedef PaddedTicketSpinLock<> lock_t;

	typedef Container::deque<nosv_task_t> waiting_tasks_t;

	static lock_t _lock;

	static commutative_mask_t _mask;

	static waiting_tasks_t _waitingTasks;

	static inline bool maskIsCompatible(const commutative_mask_t candidate)
	{
		return ((_mask & candidate) == 0);
	}

	static inline void maskRegister(const commutative_mask_t mask)
	{
		_mask |= mask;
	}

	static inline void maskRelease(const commutative_mask_t mask)
	{
		_mask &= ~mask;
	}

	//! Single-qword round of MurmurHash3
	static inline unsigned long long addressHash(void *address)
	{
		unsigned long long k = (unsigned long long)address;

		k ^= k >> 33;
		k *= 0xff51afd7ed558ccdLLU;
		k ^= k >> 33;
		k *= 0xc4ceb9fe1a85ec53LLU;
		k ^= k >> 33;

		return k;
	}
};

#endif // COMMUTATIVE_SEMAPHORE_HPP
