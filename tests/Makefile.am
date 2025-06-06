#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021-2024 Barcelona Supercomputing Center (BSC)

.NOTPARALLEL:

AM_CXXFLAGS = -fdo-not-use-ompss-2-runtime -I$(top_srcdir)/tests/common $(PTHREAD_CFLAGS) -I$(top_srcdir)/api -I$(top_builddir) -include nodes.h
# Note: Local libs must precede $(nosv_LIBS), since $(nosv_LIBS) may also have a NODES installation.
#       Moreover, since clang may add paths before ours, we must precede the local paths with "-Wl,-rpath" so they have preference
AM_LDFLAGS  = -L$(top_builddir)/.libs -Wl,-rpath $(abs_top_builddir)/.libs $(nosv_LIBS)
LDADD = $(top_builddir)/nodes-main-wrapper.o $(top_builddir)/.libs/libnodes.so -ldl

CXX = $(NODES_CLANGXX)
CC = $(NODES_CLANG)

if HAVE_NODES_CLANG
correctness_tests = \
	blocking.test \
	commutative.test \
	deadline.test \
	discrete-deps.test \
	discrete-deps-early-release.test \
	discrete-deps-er-and-weak.test \
	discrete-deps-nonest.test \
	discrete-deps-release.test \
	discrete-deps-wait.test \
	events.test \
	events-dep.test \
	fibonacci.test \
	if0.test \
	red-firstprivate.test \
	red-nest.test \
	red-nest-other.test \
	red-nonest.test \
	red-nqueens.test \
	red-stress.test \
	taskiter-for.test \
	taskiter-unroll.test \
	taskiter-while.test \
	taskloop-dep-multiaxpy.test \
	taskloop-multiaxpy.test \
	taskloop-nested-dep-multiaxpy.test \
	taskloop-nonpod.test \
	taskloop-nqueens.test

if HAVE_CXX_20
correctness_tests += \
	critical_awaitable.test \
	deadline_awaitable.test \
	fibonacci_coroutine.test \
	suspend.test
endif

endif


check_PROGRAMS = $(correctness_tests)
TESTS = $(correctness_tests)

blocking_test_SOURCES  = correctness/blocking/blocking.cpp
blocking_test_CXXFLAGS = $(AM_CXXFLAGS)
blocking_test_LDFLAGS  = $(AM_LDFLAGS)

commutative_test_SOURCES  = correctness/commutative/commutative.cpp
commutative_test_CXXFLAGS = $(AM_CXXFLAGS)
commutative_test_LDFLAGS  = $(AM_LDFLAGS)

deadline_test_SOURCES  = correctness/deadline/deadline.cpp
deadline_test_CXXFLAGS = $(AM_CXXFLAGS)
deadline_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_test_SOURCES  = correctness/dependencies/discrete-deps.cpp
discrete_deps_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_early_release_test_SOURCES  = correctness/dependencies/discrete-deps-early-release.cpp
discrete_deps_early_release_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_early_release_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_er_and_weak_test_SOURCES  = correctness/dependencies/discrete-deps-er-and-weak.cpp
discrete_deps_er_and_weak_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_er_and_weak_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_nonest_test_SOURCES  = correctness/dependencies/discrete-deps-nonest.cpp
discrete_deps_nonest_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_nonest_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_release_test_SOURCES  = correctness/dependencies/discrete-deps-release.cpp
discrete_deps_release_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_release_test_LDFLAGS  = $(AM_LDFLAGS)

discrete_deps_wait_test_SOURCES  = correctness/dependencies/discrete-deps-wait.cpp
discrete_deps_wait_test_CXXFLAGS = $(AM_CXXFLAGS)
discrete_deps_wait_test_LDFLAGS  = $(AM_LDFLAGS)

events_test_SOURCES  = correctness/events/events.cpp
events_test_CXXFLAGS = $(AM_CXXFLAGS)
events_test_LDFLAGS  = $(AM_LDFLAGS)

events_dep_test_SOURCES  = correctness/events/events-dep.cpp
events_dep_test_CXXFLAGS = $(AM_CXXFLAGS)
events_dep_test_LDFLAGS  = $(AM_LDFLAGS)

fibonacci_test_SOURCES  = correctness/fibonacci/fibonacci.cpp
fibonacci_test_CXXFLAGS = $(AM_CXXFLAGS)
fibonacci_test_LDFLAGS  = $(AM_LDFLAGS)

if0_test_SOURCES  = correctness/dependencies/if0.cpp
if0_test_CXXFLAGS = $(AM_CXXFLAGS)
if0_test_LDFLAGS  = $(AM_LDFLAGS)

red_firstprivate_test_SOURCES  = correctness/reductions/red-firstprivate.cpp
red_firstprivate_test_CXXFLAGS = $(AM_CXXFLAGS)
red_firstprivate_test_LDFLAGS  = $(AM_LDFLAGS)

red_nest_test_SOURCES  = correctness/reductions/red-nest.cpp
red_nest_test_CXXFLAGS = $(AM_CXXFLAGS)
red_nest_test_LDFLAGS  = $(AM_LDFLAGS)

red_nest_other_test_SOURCES  = correctness/reductions/red-nest-other.cpp
red_nest_other_test_CXXFLAGS = $(AM_CXXFLAGS)
red_nest_other_test_LDFLAGS  = $(AM_LDFLAGS)

red_nonest_test_SOURCES  = correctness/reductions/red-nonest.cpp
red_nonest_test_CXXFLAGS = $(AM_CXXFLAGS)
red_nonest_test_LDFLAGS  = $(AM_LDFLAGS)

red_nqueens_test_SOURCES  = correctness/reductions/red-nqueens.cpp
red_nqueens_test_CXXFLAGS = $(AM_CXXFLAGS)
red_nqueens_test_LDFLAGS  = $(AM_LDFLAGS)

red_stress_test_SOURCES  = correctness/reductions/red-stress.cpp
red_stress_test_CXXFLAGS = $(AM_CXXFLAGS)
red_stress_test_LDFLAGS  = $(AM_LDFLAGS)

taskiter_for_test_SOURCES  = correctness/taskiter/taskiter-for.cpp
taskiter_for_test_CXXFLAGS = $(AM_CXXFLAGS)
taskiter_for_test_LDFLAGS  = $(AM_LDFLAGS)

taskiter_unroll_test_SOURCES  = correctness/taskiter/taskiter-unroll.cpp
taskiter_unroll_test_CXXFLAGS = $(AM_CXXFLAGS)
taskiter_unroll_test_LDFLAGS  = $(AM_LDFLAGS)

taskiter_while_test_SOURCES  = correctness/taskiter/taskiter-while.cpp
taskiter_while_test_CXXFLAGS = $(AM_CXXFLAGS)
taskiter_while_test_LDFLAGS  = $(AM_LDFLAGS)

taskloop_multiaxpy_test_SOURCES  = correctness/taskloop/taskloop-multiaxpy.cpp
taskloop_multiaxpy_test_CXXFLAGS = $(AM_CXXFLAGS)
taskloop_multiaxpy_test_LDFLAGS  = $(AM_LDFLAGS)

taskloop_dep_multiaxpy_test_SOURCES  = correctness/taskloop/taskloop-dep-multiaxpy.cpp
taskloop_dep_multiaxpy_test_CXXFLAGS = $(AM_CXXFLAGS)
taskloop_dep_multiaxpy_test_LDFLAGS  = $(AM_LDFLAGS)

taskloop_nested_dep_multiaxpy_test_SOURCES  = correctness/taskloop/taskloop-nested-dep-multiaxpy.cpp
taskloop_nested_dep_multiaxpy_test_CXXFLAGS = $(AM_CXXFLAGS)
taskloop_nested_dep_multiaxpy_test_LDFLAGS  = $(AM_LDFLAGS)

taskloop_nonpod_test_SOURCES  = correctness/taskloop/taskloop-nonpod.cpp
taskloop_nonpod_test_CXXFLAGS = $(AM_CXXFLAGS)
taskloop_nonpod_test_LDFLAGS  = $(AM_LDFLAGS)

taskloop_nqueens_test_SOURCES  = correctness/taskloop/taskloop-nqueens.cpp
taskloop_nqueens_test_CXXFLAGS = $(AM_CXXFLAGS)
taskloop_nqueens_test_LDFLAGS  = $(AM_LDFLAGS)

if HAVE_CXX_20
critical_awaitable_test_SOURCES  = correctness/coroutine/critical_awaitable.cpp
critical_awaitable_test_CXXFLAGS = $(AM_CXXFLAGS) -std=c++20 -fcoroutines
critical_awaitable_test_LDFLAGS  = $(AM_LDFLAGS)

deadline_awaitable_test_SOURCES  = correctness/coroutine/deadline_awaitable.cpp
deadline_awaitable_test_CXXFLAGS = $(AM_CXXFLAGS) -std=c++20 -fcoroutines
deadline_awaitable_test_LDFLAGS  = $(AM_LDFLAGS)

fibonacci_coroutine_test_SOURCES  = correctness/coroutine/fibonacci_coroutine.cpp
fibonacci_coroutine_test_CXXFLAGS = $(AM_CXXFLAGS) -std=c++20 -fcoroutines
fibonacci_coroutine_test_LDFLAGS  = $(AM_LDFLAGS)

suspend_test_SOURCES  = correctness/coroutine/suspend.cpp
suspend_test_CXXFLAGS = $(AM_CXXFLAGS) -std=c++20 -fcoroutines
suspend_test_LDFLAGS  = $(AM_LDFLAGS)
endif


TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tests/tap-driver.sh
EXTRA_DIST = tap-driver.sh $(TESTS)

build-tests-local: $(check_PROGRAMS)
