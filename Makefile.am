#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(ovni_CPPFLAGS) $(BOOST_CPPFLAGS) $(PTHREAD_CFLAGS) ${nosv_CPPFLAGS} ${nanos6_CXXFLAGS} -I$(srcdir)/api -I$(top_srcdir)/src -I$(builddir)/src
AM_CXXFLAGS = $(ovni_CXXFLAGS) $(BOOST_CXXFLAGS) $(PTHREAD_CFLAGS) ${nosv_CPPFLAGS} ${nanos6_CXXFLAGS} -I$(srcdir)/api -I$(top_srcdir)/src -I$(builddir)/src
AM_LDFLAGS  = $(ovni_LIBS) $(BOOST_LDFLAGS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) ${nosv_LIBS}
AM_LIBS     = -lrt -lpthread -lnosv

SUBDIRS = . tests

# See info page of libtool "Updating version info"
lib_current = 0
lib_revision = 0
lib_age = 0

# Expected shared object numbers
lib_major = $(shell expr $(lib_current) - $(lib_age))
lib_suffix = $(lib_major).$(lib_age).$(lib_revision)

nanos6includedir = $(includedir)/nanos6

nanos6include_HEADERS = \
	api/nanos6/blocking.h \
	api/nanos6/bootstrap.h \
	api/nanos6/constants.h \
	api/nanos6/debug.h \
	api/nanos6/dependencies.h \
	api/nanos6/events.h \
	api/nanos6/final.h \
	api/nanos6/library-mode.h \
	api/nanos6/loop.h \
	api/nanos6/major.h \
	api/nanos6/reductions.h \
	api/nanos6/task-info-registration.h \
	api/nanos6/task-instantiation.h \
	api/nanos6/taskwait.h \
	api/nanos6/user-mutex.h

nodist_nanos6include_HEADERS = \
	nanos6/multidimensional-dependencies.h \
	nanos6/multidimensional-release.h

include_HEADERS = \
	api/nanos6.h

noinst_HEADERS = \
	src/common/AtomicBitset.hpp \
	src/common/Chrono.hpp \
	src/common/Containers.hpp \
	src/common/ErrorHandler.hpp \
	src/common/MathSupport.hpp \
	src/common/PaddedSpinLock.hpp \
	src/common/PaddedTicketSpinLock.hpp \
	src/common/Padding.hpp \
	src/common/SpinLock.hpp \
	src/common/SpinWait.hpp \
	src/common/TicketSpinLock.hpp \
	src/common/UserMutex.hpp \
	src/dependencies/DataAccessBase.hpp \
	src/dependencies/DataAccessRegion.hpp \
	src/dependencies/DataAccessType.hpp \
	src/dependencies/DataTrackingSupport.hpp \
	src/dependencies/MultidimensionalAPITraversal.hpp \
	src/dependencies/SymbolTranslation.hpp \
	src/dependencies/discrete/BottomMapEntry.hpp \
	src/dependencies/discrete/CPUDependencyData.hpp \
	src/dependencies/discrete/CommutativeSemaphore.hpp \
	src/dependencies/discrete/DataAccess.hpp \
	src/dependencies/discrete/DataAccessFlags.hpp \
	src/dependencies/discrete/DataAccessRegistration.hpp \
	src/dependencies/discrete/DependencySystem.hpp \
	src/dependencies/discrete/DeviceReductionStorage.hpp \
	src/dependencies/discrete/MultidimensionalAPI.hpp \
	src/dependencies/discrete/ReductionInfo.hpp \
	src/dependencies/discrete/ReductionSpecific.hpp \
	src/dependencies/discrete/TaskDataAccesses.hpp \
	src/dependencies/discrete/TaskDataAccessesInfo.hpp \
	src/dependencies/discrete/devices/HostReductionStorage.hpp \
	src/hardware/HardwareInfo.hpp \
	src/instrument/OVNIInstrumentation.hpp \
	src/memory/MemoryAllocator.hpp \
	src/memory/ObjectAllocator.hpp \
	src/system/SpawnFunction.hpp \
	src/system/TaskFinalization.hpp \
	src/tasks/TaskInfo.hpp \
	src/tasks/TaskMetadata.hpp \
	src/tasks/TaskloopMetadata.hpp \
	tests/common/Atomic.hpp \
	tests/common/Functors.hpp \
	tests/common/TAPDriver.hpp \
	tests/common/Timer.hpp

common_sources = \
	src/bootstrap/Initialization.cpp \
	src/common/ErrorHandler.cpp \
	src/common/SpinLock.cpp \
	src/dependencies/DataTrackingSupport.cpp \
	src/dependencies/discrete/CPUDependencyData.cpp \
	src/dependencies/discrete/CommutativeSemaphore.cpp \
	src/dependencies/discrete/DataAccess.cpp \
	src/dependencies/discrete/DataAccessRegistration.cpp \
	src/dependencies/discrete/ReductionInfo.cpp \
	src/dependencies/discrete/RegisterDependencies.cpp \
	src/dependencies/discrete/ReleaseDirective.cpp \
	src/dependencies/discrete/devices/HostReductionStorage.cpp \
	src/hardware/HardwareInfo.cpp \
	src/system/DebugAPI.cpp \
	src/system/EventsAPI.cpp \
	src/system/SpawnFunction.cpp \
	src/system/TaskBlocking.cpp \
	src/system/TaskCreation.cpp \
	src/system/TaskFinalization.cpp \
	src/system/TaskWait.cpp \
	src/tasks/Task.cpp \
	src/tasks/TaskInfo.cpp \
	src/tasks/TaskInfoAPI.cpp \
	src/tasks/TaskloopMetadata.cpp

dependency_flags = -I$(srcdir)/src/dependencies -I$(srcdir)/src/dependencies/discrete

lib_LTLIBRARIES = libnanos6.la

libnanos6_la_CPPFLAGS = $(AM_CPPFLAGS)
libnanos6_la_LDFLAGS  = $(AM_LDFLAGS)
libnanos6_la_SOURCES  = $(common_sources)
libnanos6_la_CXXFLAGS = $(AM_CXXFLAGS) $(dependency_flags)

#
# Taskification of the "main" function
#

noinst_LIBRARIES = libnanos6-main-wrapper.a
lib_OBJECTS = nanos6-main-wrapper.o

libnanos6_main_wrapper_a_SOURCES = \
	src/bootstrap/intercept-main-common.c \
	src/bootstrap/intercept-main-common.h \
	src/bootstrap/intercept-main-glibc.c \
	src/bootstrap/main-wrapper.c \
	src/bootstrap/main-wrapper.h

libnanos6_main_wrapper_a_CPPFLAGS = $(MCMODEL_FLAGS)

nanos6-main-wrapper.o: libnanos6-main-wrapper.a
	$(AM_V_LD)$(LD) -r --whole-archive libnanos6-main-wrapper.a -o nanos6-main-wrapper.o


#
# Multidimensional API
#

nanos6_generated_headers = \
	nanos6/multidimensional-dependencies.h \
	nanos6/multidimensional-release.h

multidimensional_adaptor_sources = MultidimensionalAPIToLinear.cpp

nodist_common_sources = $(nanos6_generated_headers) $(multidimensional_adaptor_sources)
nodist_libnanos6_la_SOURCES = $(nodist_common_sources)

BUILT_SOURCES = $(nanos6_generated_headers) $(multidimensional_adaptor_sources)
CLEANFILES = $(nodist_libnanos6_la_SOURCES) $(multidimensional_adaptor_sources)

#
# Automatically generated API headers
#

MAX_REGION_DIMENSIONS = 8
MULTIDIMENSIONAL_REGION_API_TYPES = read write readwrite weak_read weak_write weak_readwrite concurrent commutative weak_commutative reduction weak_reduction
SUPPORTED_MULTIDIMENSIONAL_REGION_API_TYPES = read write readwrite weak_read weak_write weak_readwrite concurrent commutative weak_commutative reduction weak_reduction


EXTRA_DIST = \
	scripts/common.sh \
	scripts/generate_multidim_functions_for_linear_regions.sh \
	scripts/generate_regions_prototypes.sh \
	scripts/generate_release_prototypes.sh \
	tests/tap-driver.sh

nanos6/multidimensional-dependencies.h: $(top_srcdir)/scripts/common.sh $(top_srcdir)/scripts/generate_regions_prototypes.sh Makefile
	$(AM_V_GEN)mkdir -p nanos6 ; \
	$(top_srcdir)/scripts/generate_regions_prototypes.sh $(MAX_REGION_DIMENSIONS) $(MULTIDIMENSIONAL_REGION_API_TYPES) > $@

nanos6/multidimensional-release.h: $(top_srcdir)/scripts/common.sh $(top_srcdir)/scripts/generate_release_prototypes.sh Makefile
	$(AM_V_GEN)mkdir -p nanos6 ; \
	$(top_srcdir)/scripts/generate_release_prototypes.sh $(MAX_REGION_DIMENSIONS) $(MULTIDIMENSIONAL_REGION_API_TYPES) > $@

MultidimensionalAPIToLinear.cpp: $(top_srcdir)/scripts/common.sh $(top_srcdir)/scripts/generate_multidim_functions_for_linear_regions.sh Makefile
	$(AM_V_GEN)$(top_srcdir)/scripts/generate_multidim_functions_for_linear_regions.sh $(MAX_REGION_DIMENSIONS) $(SUPPORTED_MULTIDIMENSIONAL_REGION_API_TYPES) > $@


all-local: $(lib_OBJECTS)

install-exec-local: $(lib_OBJECTS)
	@$(NORMAL_INSTALL)
	@list='$(lib_OBJECTS)'; test -n "$(libdir)" || list=; \
	list2=; for p in $$list; do \
		if test -f $$p; then \
		list2="$$list2 $$p"; \
		else :; fi; \
	done; \
	test -z "$$list2" || { \
		echo " $(MKDIR_P) '$(DESTDIR)$(libdir)'"; \
		$(MKDIR_P) "$(DESTDIR)$(libdir)" || exit 1; \
		echo " $(INSTALL) $$list2 '$(DESTDIR)$(libdir)'"; \
		$(INSTALL) $$list2 "$(DESTDIR)$(libdir)"; \
	}

uninstall-hook:
	for l in $(lib_OBJECTS) ; do \
		if test -f "$(DESTDIR)$(libdir)/$$l" ; then \
			$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=uninstall rm -f "$(DESTDIR)$(libdir)/$$l" ; \
		fi ; \
	done

# All potential files in this list will be distributed
# regardless of the Makefile conditionals
noinst_HEADERS += \
	$(common_sources)

build-tests-local: $(check_PROGRAMS)
