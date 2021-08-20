#	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(PTHREAD_CFLAGS) ${nosv_CPPFLAGS} -I$(top_srcdir)/src -I$(top_srcdir)/src/common
AM_CXXFLAGS = $(BOOST_CXXFLAGS) $(PTHREAD_CFLAGS) ${nosv_CPPFLAGS}
AM_LDFLAGS  = $(BOOST_LDFLAGS)  $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) ${nosv_LIBS}
AM_LIBS     = -lrt -lpthread -lnosv

# See info page of libtool "Updating version info"
lib_current = 0
lib_revision = 0
lib_age = 0

# Expected shared object numbers
lib_major = $(shell expr $(lib_current) - $(lib_age))
lib_suffix = $(lib_major).$(lib_age).$(lib_revision)

include_HEADERS = \
	api/bootstrap.h \
	api/constants.h \
	api/final.h \
	api/library-mode.h \
	api/major.h \
	api/nanos6.h \
	api/task-info-registration.h \
	api/task-instantiation.h \
	api/taskwait.h

noinst_HEADERS = \
	src/common/ErrorHandler.hpp \
	src/common/SpinLock.hpp \
	src/common/SpinWait.hpp \
	src/system/SpawnFunction.hpp \
	src/system/TaskCreation.hpp \
	src/system/TaskFinalization.hpp \
	src/tasks/TaskInfo.hpp

common_sources = \
	src/common/ErrorHandler.cpp \
	src/common/SpinLock.cpp \
	src/system/Bootstrap.cpp \
	src/system/SpawnFunction.cpp \
	src/system/TaskCreation.cpp \
	src/system/TaskFinalization.cpp \
	src/system/TaskWait.cpp \
	src/tasks/Task.cpp \
	src/tasks/TaskInfo.cpp \
	src/tasks/TaskInfoAPI.cpp

lib_LTLIBRARIES = libnanos6.la

libnanos6_la_CPPFLAGS = $(AM_CPPFLAGS)
libnanos6_la_SOURCES  = $(common_sources)


#
# Taskification of the "main" function
#

noinst_LIBRARIES = libnanos6-main-wrapper.a
lib_OBJECTS = nanos6-main-wrapper.o

libnanos6_main_wrapper_a_SOURCES = \
	src/main/intercept-main-common.c \
	src/main/intercept-main-glibc.c \
	src/main/main-wrapper.c \
	src/main/main-wrapper.h

libnanos6_main_wrapper_a_CPPFLAGS = $(MCMODEL_FLAGS)

nanos6-main-wrapper.o: libnanos6-main-wrapper.a
	$(AM_V_LD)$(LD) -r --whole-archive libnanos6-main-wrapper.a -o nanos6-main-wrapper.o


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
