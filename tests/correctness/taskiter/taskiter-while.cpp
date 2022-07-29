/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include <iostream>

#include "TAPDriver.hpp"

#define ITERATIONS 100
#define WIDTH 50

int testArray[WIDTH];

TAPDriver tap;

int main() {
	int i = 0;

	#pragma oss taskiter shared(testArray) firstprivate(i)
	while (i < ITERATIONS) {
		for (int j = 0; j < WIDTH; ++j) {
			#pragma oss task shared(testArray) firstprivate(j) inout(testArray[j]) in(i)
			{
				testArray[j]++;
			}

			#pragma oss task shared(testArray) firstprivate(j) inout(testArray[j]) in(i)
			{
				testArray[j]++;
			}
		}

		#pragma oss task shared(i) inout(i)
		{
			i++;
		}
	}

	#pragma oss taskwait

	bool validated = true;
	for (int j = 0; j < WIDTH; ++j) {
		if (testArray[j] != 2*ITERATIONS) {
			validated = false;
			break;
		}
	}

	tap.evaluate(validated, "The result of the arrays is correct");
	tap.end();

	return 0;
}
