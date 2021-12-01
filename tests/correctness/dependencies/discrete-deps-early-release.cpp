/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

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


#define SUSTAIN_MICROSECONDS 100000L

TAPDriver tap;

struct ExperimentStatus {
	Atomic<bool> _t1_HasStarted;
	Atomic<bool> _t1_HasFinished;
	Atomic<bool> _t1_1_HasStarted;
	Atomic<bool> _t1_1_HasFinished;
	Atomic<bool> _t2_HasStarted;
	Atomic<bool> _t2_HasFinished;

	ExperimentStatus()
		: _t1_HasStarted(false), _t1_HasFinished(false),
		_t1_1_HasStarted(false), _t1_1_HasFinished(false),
		_t2_HasStarted(false), _t2_HasFinished(false)
	{
	}
};

struct ExpectedOutcome {
	bool _t2_waits_t1;
	bool _t2_waits_t1_1;
};

static void t1_verification(ExperimentStatus &status, ExpectedOutcome &expected)
{
	if (expected._t2_waits_t1) {
		tap.sustainedEvaluate(False< Atomic<bool> >(status._t2_HasStarted), SUSTAIN_MICROSECONDS, "Evaluating that T2 does not start before T1 finishes");
	} else {
		tap.timedEvaluate(True< Atomic<bool> >(status._t2_HasFinished), SUSTAIN_MICROSECONDS, "Evaluating that T2 can finish before T1 finishes");
	}
}

static void t1_1_verification(ExperimentStatus &status, ExpectedOutcome &expected)
{
	if (expected._t2_waits_t1_1) {
		tap.sustainedEvaluate(False< Atomic<bool> >(status._t2_HasStarted), SUSTAIN_MICROSECONDS*2L, "Evaluating that T2 does not start before T1_1 finishes");
	} else {
		tap.timedEvaluate(True< Atomic<bool> >(status._t2_HasFinished), SUSTAIN_MICROSECONDS*2L, "Evaluating that T2 can finish before T1_1 finishes");
	}
}

static void t2_verification(ExperimentStatus &status, ExpectedOutcome &expected)
{
	if (expected._t2_waits_t1) {
		tap.evaluate(status._t1_HasFinished.load(), "Evaluating that when T2 starts T1 has finished");
	} else {
		tap.evaluate(!status._t1_HasFinished.load(), "Evaluating that when T2 starts T1 has not finished");
	}

	if (expected._t2_waits_t1_1) {
		tap.evaluate(status._t1_1_HasFinished.load(), "Evaluating that when T2 starts T1_1 has finished");
	} else {
		tap.evaluate(!status._t1_1_HasFinished.load(), "Evaluating that when T2 starts T1_1 has not finished");
	}
}

int main(int argc, char **argv)
{
	long activeCPUs = nanos6_get_total_num_cpus();
	if (activeCPUs <= 2) {
		tap.skip("This test requires more than 2 CPUs");
		tap.end();
		return 0;
	}

	int var1, var2;

	// Test 1
	// R1,W2 {W2} -- W1
	{
		tap.emitDiagnostic("Test 1:   R1,W2 {W2} -- W1");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, false};

		#pragma oss task shared(var1, var2, status, expected) in(var1) out(var2) label ("T1 R1 W2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) out(var2) label ("T1_1 W2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected); // 1
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected); // 2
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) out(var1) label ("T2 W1")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected); // 3, 4
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 2
	// RW1 {R1} -- R1
	{
		tap.emitDiagnostic("Test 2:   RW1 {R1} -- R1");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, false};

		#pragma oss task shared(var1, var2, status, expected) inout(var1) label("T1 RW1")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) in(var1) label("T1_1 R1")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected);  // 5
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected); // 6
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var1) label("T2 R1")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected); // 7,8
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 3
	// W1,W2 {W2} -- R1
	{
		tap.emitDiagnostic("Test 3:   W1,W2 {W2} -- R1");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, false};

		#pragma oss task shared(var1, var2, status, expected) out(var1, var2) label("T1 W1 W2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) out(var2) label("T1_1 W2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected); // 9
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected); // 10
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var1) label("T2 R1")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected); // 11,12
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 4
	// W1,R2 {R2} -- R1,R2
	{
		tap.emitDiagnostic("Test 4:   W1,R2 {R2} -- R1,R2");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, false};

		#pragma oss task shared(var1, var2, status, expected) in(var2) out(var1) label("T1 W1 R2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) in(var2) label("T1_1 W2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected); // 13
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected); // 14
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var1, var2) label("T2 R1 R2")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected); // 15, 16
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 5
	// W1 {W1} -- W1
	{
		tap.emitDiagnostic("Test 5:   W1 {W1} -- W1");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, true};

		#pragma oss task shared(var1, var2, status, expected) out(var1) label("T1 W1")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) out(var1) label("T1_1 W1")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected); // 17
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected); // 18
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) out(var1) label("T2 W1")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected); // 19,20
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 6
	// R1,R2 {R2} -- R1
	{
		tap.emitDiagnostic("Test 6:   R1,R2 {R2} -- R1");
		ExperimentStatus status;
		ExpectedOutcome expected = {false, false};

		#pragma oss task shared(var1, var2, status, expected) in(var1,var2) label("T1 R1 R2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) in(var2) label("T1_1 W2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected);
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected);
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var1) label("T2 R1")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected);
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 7
	// R1,R2 {R2} -- R2
	{
		tap.emitDiagnostic("Test 7:   R1,R2 {R2} -- R2");
		ExperimentStatus status;
		ExpectedOutcome expected = {false, false};

		#pragma oss task shared(var1, var2, status, expected) in(var1,var2) label("T1 R1 R2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) in(var2) label("T1_1 R2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected);
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected);
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var2) label("T2 R2")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected);
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}


	// Test 8
	// W1,W2 {W2} -- R2
	{
		tap.emitDiagnostic("Test 8:   W1,W2 {W2} -- R2");
		ExperimentStatus status;
		ExpectedOutcome expected = {true, true};

		#pragma oss task shared(var1, var2, status, expected) out(var1, var2) label("T1 W1 W2")
		{
			status._t1_HasStarted.store(true);
			#pragma oss task shared(var1, var2, status, expected) out(var2) label("T1_1 W2")
			{
				status._t1_1_HasStarted.store(true);
				t1_1_verification(status, expected);
				status._t1_1_HasFinished.store(true);
			}
			t1_verification(status, expected);
			status._t1_HasFinished.store(true);
		}
		#pragma oss task shared(var1, var2, status, expected) in(var2) label("T2 R2")
		{
			status._t2_HasStarted.store(true);
			t2_verification(status, expected);
			status._t2_HasFinished.store(true);
		}
		#pragma oss taskwait
	}

	tap.end();

	return 0;
}

