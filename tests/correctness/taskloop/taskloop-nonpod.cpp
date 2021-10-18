/*
	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "TAPDriver.hpp"


#define N 100

TAPDriver tap;

int main() {
	std::vector<int> v(N, N);
	int i;

	#pragma oss taskloop firstprivate(v)
	for (i = 0; i < N; ++i) {
		v[i]++;
	}
	#pragma oss taskwait

	bool correct = true;
	for (i = 0; i < N; ++i) {
		if (v[i] != N) {
			correct = false;
		}
	}

	tap.evaluate(correct, "Program finished correctly");
	tap.end();

	return 0;
}
