#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#	This file is part of Nanos6-Lite and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)

# Nanos6-Lite Version and Copyright

m4_define([nanos6lite_version], [1.0.0])
m4_define([nanos6lite_license], ["GPL3"])
m4_define([nanos6lite_copyright], ["2021 Barcelona Supercomputing Center (BSC)"])

AC_PREREQ([2.69])
AC_INIT([Nanos6-Lite], [nanos6lite_version], [antoni.navarro@bsc.es])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_LANG([C++])

# Prepare compilation flags before AC_PROG_CXX
AX_COMPILE_FLAGS

# Retrieve compiler version
AX_CHECK_CXX_VERSION

# AM_PROG_AR must be called before LT_INIT or a warning ensues
AM_PROG_AR

# Automake initialization
AM_INIT_AUTOMAKE([foreign -Wall dist-bzip2 -Wno-portability subdir-objects silent-rules])
AM_SILENT_RULES([yes])

# Checks for header files
AC_CHECK_HEADERS([unistd.h])

# Checks for libraries
AX_BOOST_BASE([1.59], [], [ AC_MSG_ERROR([Boost library not found]) ])
CHECK_PTHREAD
AC_CHECK_NOSV

# Checks for typedefs, structures, and compiler characteristics
AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

LT_INIT([shared disable-static pic-only])

# Enable/disable architecture specific code
AS_CASE([$host], [x86_64*], [ARCH=x86_64])
AS_IF([test "$ARCH" = "x86_64"], [
	AC_DEFINE([X86_64_ARCH], [1], [Compiling for x86_64 host])
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo ""
echo ""
echo "Configuration summary:"
echo "    Compiler version...    ${CXX_VERSION}"
echo ""
echo "    Installation prefix... ${prefix}"
echo ""
echo "    CXXFLAGS...            ${CXXFLAGS}"
echo "    CPPFLAGS...            ${CPPFLAGS}"
echo ""
echo "    Boost CPPFLAGS...      ${BOOST_CPPFLAGS}"
echo "    Boost LDFLAGS...       ${BOOST_LDFLAGS}"
echo ""
echo "    nOS-V CPPFLAGS...      ${nosv_CPPFLAGS}"
