/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2024 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <vector>

#include <nodes.h>

#include "Atomic.hpp"
#include "TAPDriver.hpp"


#define NUM_TASKS 100
#define NUM_BLOCKS 50

typedef std::vector<Atomic<void *> > contexts_list_t;
typedef std::vector<Atomic<int> > processed_list_t;

struct UnblockerArgs {
	contexts_list_t *_contexts;
	processed_list_t *_processed;
	int _nblocks;

	UnblockerArgs(contexts_list_t *contexts, processed_list_t *processed, int nblocks) :
		_contexts(contexts),
		_processed(processed),
		_nblocks(nblocks)
	{
	}
};

TAPDriver tap;


void *unblocker(void *arg)
{
	UnblockerArgs *args = (UnblockerArgs *) arg;
	assert(args != NULL);

	const int nblocks = args->_nblocks;
	contexts_list_t &contexts = *args->_contexts;
	processed_list_t &processed = *args->_processed;

	// Iterate through all contexts unblocking the tasks
	for (int b = 0; b < nblocks; ++b) {
		for (int c = 0; c < contexts.size(); ++c) {
			void *context = NULL;
			while (!(context = contexts[c]));
			++processed[c];
			contexts[c] = NULL;
			nanos6_unblock_task(context);
		}
	}
	return NULL;
}

#pragma oss task
oss_coroutine blocking_task(int task, int nblocks, contexts_list_t *contexts, processed_list_t *processed)
{
	for (int b = 0; b < nblocks; ++b) {
		void *context = nanos6_get_current_blocking_context();
		assert(context != NULL);

		// Save the blocking context so the unblocker can
		// unblock this task eventually
		(*contexts)[task] = context;

		// Suspend the current task
		co_await oss_co_suspend_current_task();

		tap.evaluate(
			(*processed)[task] == b + 1,
			"Check that the task was correctly unblocked"
		);

		assert((*contexts)[task] == NULL);
	}
}

int main(int argc, char **argv)
{
	const long activeCPUs = nanos6_get_total_num_cpus();
	tap.emitDiagnostic("Detected ", activeCPUs, " CPUs");

	if (activeCPUs == 1) {
		// This test only works correctly with more than 1 CPU
		tap.skip("This test does not work with just 1 CPU");
		tap.end();
		return 0;
	}

	const int ntasks = NUM_TASKS;
	const int nblocks = NUM_BLOCKS;
	contexts_list_t contexts(ntasks);
	processed_list_t processed(ntasks);
	for (int c = 0; c < ntasks; ++c) {
		contexts[c] = NULL;
		processed[c] = 0;
	}

	// Create an external thread that will unblock all tasks
	pthread_t thread;
	UnblockerArgs args(&contexts, &processed, nblocks);
	pthread_create(&thread, NULL, unblocker, &args);

	for (int task = 0; task < ntasks; ++task) {
		blocking_task(task, nblocks, &contexts, &processed);
	}
	#pragma oss taskwait

	// Check that all contexts were processed by the unblocker
	for (int c = 0; c < ntasks; ++c) {
		tap.evaluate(
			processed[c] == nblocks,
			"Check that the task was correctly unblocked all times"
		);
	}

	pthread_join(thread, NULL);

	tap.end();

	return 0;
}
