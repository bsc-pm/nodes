/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_HPP
#define DATA_ACCESS_HPP

#include <atomic>
#include <bitset>
#include <cassert>
#include <iostream>
#include <stack>

#include <nosv.h>

#include "DataAccessFlags.hpp"
#include "ReductionInfo.hpp"
#include "ReductionSpecific.hpp"
#include "dependencies/DataAccessType.hpp"
#include "dependencies/DataTrackingSupport.hpp"


//! The accesses that one or more tasks perform sequentially to a memory location that can occur concurrently (unless commutative)
//! WARNING: When modifying this structure, please mind to pack it as much as possible
//! There might be thousands of allocations of this struct, and size will have a noticeable effect on performance
struct DataAccess {

public:

	static const size_t MAX_SYMBOLS = 64;
	typedef std::bitset<MAX_SYMBOLS> symbols_t;

private:

	//! 16-byte fields
	//! The region covered by the access
	DataAccessRegion _region;

	//! 8-byte fields
	//! The originator of the access
	nosv_task_t _originator;

	//! A bitmap of the "symbols" this access is related to
	symbols_t _symbols;

	//! Reduction-specific information of current access
	ReductionInfo *_reductionInfo;

	//! Next task with an access matching this one
	std::atomic<DataAccess *> _successor;
	std::atomic<DataAccess *> _child;

	//! 4-byte fields
	//! Reduction information
	reduction_type_and_operator_index_t _reductionOperator;
	reduction_index_t _reductionIndex;

	//! Atomic flags for Read / Write / Deletable / Finished
	std::atomic<access_flags_t> _accessFlags;

	//! 1-byte fields
	//! The type of the access
	DataAccessType _type;

	DataAccessMessage inAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	void outAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message, bool weak);
	void inoutAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message, bool weak);
	void reductionAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message);
	DataAccessMessage concurrentAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	DataAccessMessage commutativeAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	void readDestination(access_flags_t allFlags, DataAccessMessage &message, PropagationDestination &destination);

public:

	DataAccess(DataAccessType type, nosv_task_t originator, void *address, size_t length, bool weak) :
		_region(address, length),
		_originator(originator),
		_reductionInfo(nullptr),
		_successor(nullptr),
		_child(nullptr),
		_accessFlags(0),
		_type(type)
	{
		assert(originator != nullptr);

		if (weak) {
			_accessFlags.fetch_or(ACCESS_IS_WEAK, std::memory_order_relaxed);
		}
	}

	DataAccess(const DataAccess &other) :
		_region(other.getAccessRegion()),
		_originator(other.getOriginator()),
		_reductionInfo(other.getReductionInfo()),
		_successor(other.getSuccessor()),
		_child(other.getChild()),
		_accessFlags(other.getFlags()),
		_type(other.getType())
	{
	}

	~DataAccess()
	{
	}

	DataAccessMessage applySingle(access_flags_t flags, mailbox_t &mailBox);

	bool apply(DataAccessMessage &message, mailbox_t &mailBox);

	bool applyPropagated(DataAccessMessage &message);

	inline void setType(DataAccessType type)
	{
		_type = type;
	}

	inline DataAccessType getType() const
	{
		return _type;
	}

	DataAccessRegion const &getAccessRegion() const
	{
		return _region;
	}

	inline nosv_task_t getOriginator() const
	{
		return _originator;
	}

	inline ReductionInfo *getReductionInfo() const
	{
		return _reductionInfo;
	}

	inline void setReductionInfo(ReductionInfo *reductionInfo)
	{
		assert(_type == REDUCTION_ACCESS_TYPE);

		_reductionInfo = reductionInfo;
	}

	inline DataAccess *getSuccessor() const
	{
		return _successor.load(std::memory_order_acquire);
	}

	inline void setSuccessor(DataAccess *successor)
	{
		_successor.store(successor, std::memory_order_release);
	}

	inline bool isWeak() const
	{
		return (_accessFlags.load(std::memory_order_relaxed) & ACCESS_IS_WEAK);
	}

	inline void setWeak(bool value = true)
	{
		if (value)
			_accessFlags.fetch_or(ACCESS_IS_WEAK, std::memory_order_relaxed);
		else
			_accessFlags.fetch_and(~ACCESS_IS_WEAK, std::memory_order_relaxed);
	}

	inline size_t getLength() const
	{
		return _region.getSize();
	}

	inline reduction_type_and_operator_index_t getReductionOperator() const
	{
		return _reductionOperator;
	}

	inline void setReductionOperator(reduction_type_and_operator_index_t reductionOperator)
	{
		_reductionOperator = reductionOperator;
	}

	inline reduction_index_t getReductionIndex() const
	{
		return _reductionIndex;
	}

	inline void setReductionIndex(reduction_index_t reductionIndex)
	{
		_reductionIndex = reductionIndex;
	}

	inline DataAccess *getChild() const
	{
		return _child.load(std::memory_order_acquire);
	}

	inline void setChild(DataAccess *child)
	{
		_child.store(child, std::memory_order_release);
	}

	inline access_flags_t getFlags() const
	{
		return _accessFlags.load(std::memory_order_relaxed);
	}

	inline bool isReleased() const
	{
		return (_accessFlags.load(std::memory_order_relaxed) & ACCESS_UNREGISTERED);
	}

	bool isInSymbol(int symbol) const
	{
		return _symbols[symbol];
	}

	void addToSymbol(int symbol)
	{
		_symbols.set(symbol);
	}

	symbols_t getSymbols() const
	{
		return _symbols;
	}

};

#endif // DATA_ACCESS_HPP
