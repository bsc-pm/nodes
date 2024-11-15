/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2024 Barcelona Supercomputing Center (BSC)
*/

#include "TAPDriver.hpp"
#include "Timer.hpp"

#include <nodes.h>


#define N 14
#define INTEGER unsigned long

TAPDriver tap;

template <unsigned long index>
struct TemplatedFibonacci {
	enum { _value = TemplatedFibonacci<index-1>::_value + TemplatedFibonacci<index-2>::_value };
};

template <>
struct TemplatedFibonacci<0> {
	enum { _value = 0 };
};

template <>
struct TemplatedFibonacci<1> {
	enum { _value = 1 };
};
	
#pragma oss task label("fibonacci")
oss_coroutine fibonacci(INTEGER index, INTEGER *resultPointer) {
	if (index <= 1) {
		*resultPointer = index;
	} else {
		INTEGER result1, result2;

		fibonacci(index-1, &result1);
		fibonacci(index-2, &result2);

		co_await oss_co_taskwait();

		*resultPointer = result1 + result2;
	}
}

int main(int argc, char **argv) {
	INTEGER result;
	Timer timer;

	fibonacci(N, &result);

	#pragma oss taskwait

	timer.stop();

	tap.emitDiagnostic("Elapsed time: ", (long int) timer, " us");

	tap.evaluate(result == TemplatedFibonacci<N>::_value, "Check if the result is correct");

	tap.end();

	return 0;
}
