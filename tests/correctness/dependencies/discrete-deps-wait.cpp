/*
	This file is part of nODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <iostream>
#include <string>
#include <unistd.h>

#include "Atomic.hpp"
#include "TAPDriver.hpp"


#define DEPTH 10000

TAPDriver tap;

Atomic<size_t> counter;

void task(int depth, int n)
{
	if (depth == n) {
		++counter;
	} else {
		#pragma oss task inout(counter) wait
		{
			#pragma oss task
			task(depth + 1, n);
		}

		#pragma oss task inout(counter)
		{
			size_t count = ++counter;
			if (depth != n - count + 1) {
				tap.evaluate(false, "The intermediate result of the program is correct");
				tap.end();
				exit(0);
			}
		}
	}
}

int main()
{
	int n = DEPTH;
	counter = 0;

	#pragma oss task
	task(0, n);

	#pragma oss taskwait

	tap.evaluate(true, "The final result of the program is correct");

	tap.end();

	return 0;
}
