/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <api/nodes/bootstrap.h>
#include <api/nodes/blocking.h>
#include <api/nodes/library-mode.h>
#include <api/nodes/taskwait.h>

#include "main-wrapper.h"


main_function_t *_nanos6_loader_wrapped_main = 0;

typedef struct {
	int argc;
	char **argv;
	char **envp;
	int returnCode;
} main_task_args_block_t;


static void main_task_wrapper(void *argsBlock)
{
	main_task_args_block_t *realArgsBlock = (main_task_args_block_t *) argsBlock;

	assert(_nanos6_loader_wrapped_main != NULL);
	assert(realArgsBlock != NULL);

	realArgsBlock->returnCode = _nanos6_loader_wrapped_main(
		realArgsBlock->argc,
		realArgsBlock->argv,
		realArgsBlock->envp
	);
}

static void main_completion_callback(void *args)
{
	void *blocking_context = args;
	assert(blocking_context != NULL);

	nanos6_unblock_task(blocking_context);
}


int _nanos6_loader_main(int argc, char **argv, char **envp)
{
	// Initialize NODES
	nanos6_init();

	// Spawn the main task
	main_task_args_block_t argsBlock = { argc, argv, envp, 0 };
	void *blocking_context = nanos6_get_current_blocking_context();
	nanos6_spawn_function(main_task_wrapper, &argsBlock, main_completion_callback, blocking_context, argv[0]);

	// Wait for the completion callback
	nanos6_block_current_task(NULL);

	// Terminate nOS-V
	nanos6_shutdown();

	return argsBlock.returnCode;
}
