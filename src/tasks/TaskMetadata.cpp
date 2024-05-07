/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2023 Barcelona Supercomputing Center (BSC)
*/

#include "TaskMetadata.hpp"

thread_local nosv_task_t TaskMetadata::_lastTask;
