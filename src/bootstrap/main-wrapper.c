/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <api/nanos6/bootstrap.h>
#include <api/nanos6/library-mode.h>
#include <api/nanos6/taskwait.h>

#include "main-wrapper.h"


main_function_t *_nanos6_loader_wrapped_main = 0;

typedef struct {
	pthread_mutex_t _mutex;
	pthread_cond_t _cond;
	int _signaled;
} condition_variable_t;

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
	condition_variable_t *condVar = (condition_variable_t *) args;
	assert(condVar != NULL);

	pthread_mutex_lock(&condVar->_mutex);
	condVar->_signaled = 1;
	pthread_cond_signal(&condVar->_cond);
	pthread_mutex_unlock(&condVar->_mutex);
}


int _nanos6_loader_main(int argc, char **argv, char **envp)
{
	// Second half of the initialization
	nanos6_init();

	condition_variable_t condVar = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};

	// Spawn the main task
	main_task_args_block_t argsBlock = { argc, argv, envp, 0 };
	nanos6_spawn_function(main_task_wrapper, &argsBlock, main_completion_callback, &condVar, argv[0]);

	// Wait for the completion callback
	pthread_mutex_lock(&condVar._mutex);
	while (condVar._signaled == 0) {
		pthread_cond_wait(&condVar._cond, &condVar._mutex);
	}
	pthread_mutex_unlock(&condVar._mutex);

	// Terminate
	nanos6_shutdown();

	return argsBlock.returnCode;
}
