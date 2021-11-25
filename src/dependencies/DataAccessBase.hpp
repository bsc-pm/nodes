/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_BASE_HPP
#define DATA_ACCESS_BASE_HPP

#include <cassert>

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

#include "DataAccessType.hpp"
#include "tasks/TaskMetadata.hpp"


struct DataAccessBase {
	#if NDEBUG
		typedef boost::intrusive::link_mode<boost::intrusive::normal_link> link_mode_t;
	#else
		typedef boost::intrusive::link_mode<boost::intrusive::safe_link> link_mode_t;
	#endif

	//! Type of access: read, write, ...
	DataAccessType _type;

	//! True iff the access is weak
	bool _weak;

	//! Tasks to which the access corresponds
	TaskMetadata *_originator;


	DataAccessBase(DataAccessType type, bool weak, TaskMetadata *originator)
		: _type(type), _weak(weak), _originator(originator)
	{
		assert(originator != nullptr);
	}
};


#endif // DATA_ACCESS_BASE_HPP
