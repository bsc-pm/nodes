/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_CREATION_HPP
#define TASK_CREATION_HPP

#define DATA_ALIGNMENT_SIZE sizeof(void *)


//! \brief Contains metadata of the task such as the args blocks
//! and other flags/attributes
struct TaskMetadata {
	//! Whether it is a spawned task
	bool _isSpawned;

	//! Args blocks of the task
	void *_argsBlock;
};

#endif // TASK_CREATION_HPP
