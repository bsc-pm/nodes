/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include <nanos6.h>

#include "Atomic.hpp"
#include "Functors.hpp"
#include "TAPDriver.hpp"
#include "Timer.hpp"

using namespace Functors;


static const int NSEGMENTS = 8;

#define SUSTAIN_MICROSECONDS 100000L

TAPDriver tap;

template <int NUM_TASKS>
struct ExperimentStatus {
	Atomic<int> _taskHasStarted[NUM_TASKS];
	Atomic<int> _taskHasFinished[NUM_TASKS];
	bool _taskHasBeenReleased[NUM_TASKS];

	ExperimentStatus()
		: _taskHasStarted(), _taskHasFinished(), _taskHasBeenReleased()
	{
		for (int i = 0; i < NUM_TASKS; i++) {
			_taskHasStarted[i].store(0);
			_taskHasFinished[i].store(0);
			_taskHasBeenReleased[i] = false;
		}
	}
};

static void verifyNonReleased(ExperimentStatus<NSEGMENTS+1> &status)
{
	for (int i = 0; i < NSEGMENTS; i++) {
		if (!status._taskHasBeenReleased[i]) {
			std::ostringstream oss;

			oss << "T" << i+1 << " does not start before its segment" << i << " has been released";

			tap.sustainedEvaluate(
				Zero< Atomic<int> >(status._taskHasStarted[i+1]),
				SUSTAIN_MICROSECONDS,
				oss.str()
			);
		}
	}
}

int main(int argc, char **argv)
{
	long activeCPUs = nanos6_get_total_num_cpus();
	if (activeCPUs < 2) {
		tap.skip("This test does not work with less than 2 CPUs");
		tap.end();
		return 0;
	}

	int var[8];

	ExperimentStatus<NSEGMENTS+1> status;

	#pragma oss task out({ var[i], i = 0; NSEGMENTS }) shared(status) label("releaser")
	{
		tap.evaluate(status._taskHasStarted[0]++ == 0, "T0 starts only once");

		verifyNonReleased(status);

		int remainingSegments = NSEGMENTS;
		while (remainingSegments > 0) {
			int segment = rand() % NSEGMENTS;

			if (!status._taskHasBeenReleased[segment]) {
				tap.emitDiagnostic("Releasing segment ", segment, " which should release T", segment+1);

				status._taskHasBeenReleased[segment] = true;

				#pragma oss release out(var[segment])

				std::ostringstream oss;
				oss << "T" << segment+1 << " starts after releasing segment " << segment;
				tap.timedEvaluate(
					One< Atomic<int> >(status._taskHasStarted[segment+1]),
					SUSTAIN_MICROSECONDS*2L,
					oss.str(),
					true
				);

				verifyNonReleased(status);

				remainingSegments--;
			}
		}

		for (int i = 0; i < NSEGMENTS; i++) {
			std::ostringstream oss;
			oss << "T" << i+1 << " can finish before T0";
			tap.timedEvaluate(
				One< Atomic<int> >(status._taskHasFinished[i+1]),
				SUSTAIN_MICROSECONDS*2L,
				oss.str(),
				true
			);
		}

		tap.evaluate(status._taskHasFinished[0]++ == 0, "T0 finishes only once");
	}

	for (int i = 0; i < NSEGMENTS; i++) {
		#pragma oss task in(var[i]) shared(status) label("released")
		{
			{
				std::ostringstream oss;
				oss << "T" << i+1 << " starts only once";
				tap.evaluate(status._taskHasStarted[i+1]++ == 0, oss.str());
			}

			tap.emitDiagnostic("T", i+1, " has started and is about to finish");


			std::ostringstream oss;
			oss << "T0 does not finish before T" << i+1;
			tap.timedEvaluate(
				Zero< Atomic<int> >(status._taskHasFinished[i+1]),
				SUSTAIN_MICROSECONDS*2L,
				oss.str(),
				true
			);

			{
				std::ostringstream oss;
				oss << "T" << i+1 << " finishes only once";
				tap.evaluate(status._taskHasFinished[i+1]++ == 0, oss.str());
			}
		}
	}

	#pragma oss taskwait

	#pragma oss task out({ var[i], i = 0; NSEGMENTS }) label("releaser only")
	{
		for (int segment = 0; segment < NSEGMENTS; segment++){
			tap.emitDiagnostic("Releasing segment ", segment);

			#pragma oss release out(var[segment])
		}
	}

	#pragma oss taskwait

	Atomic<bool> secondHasFinished(false);

	#pragma oss task weakout({ var[i], i = 0; NSEGMENTS }) shared(secondHasFinished) label("weak waiter")
	{
		tap.emitDiagnostic("T0 waitig fror T1 to finish");
		while (!secondHasFinished.load()) {
			// Keep waiting
		}
		tap.emitDiagnostic("T0 can proceed");
	}

	#pragma oss task weakout({ var[i], i = 0; NSEGMENTS }) shared(secondHasFinished) label("weak releaser")
	{
		tap.emitDiagnostic("T1 starts");
		for (int segment = 0; segment < NSEGMENTS; segment++){
			tap.emitDiagnostic("T1 Releasing segment ", segment);

			#pragma oss release weakout(var[segment])
			// nanos6_release_weak_write_1(&var[segment], sizeof(var[segment]), 0, sizeof(var[segment]));
		}
		tap.emitDiagnostic("T1 finishes");
		secondHasFinished.store(true);
	}

	#pragma oss taskwait

	tap.end();

	return 0;
}

