#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)

# NODES Version and Copyright

m4_define([nodes_version], [1.1.0])
m4_define([nodes_license], ["GPL3"])
m4_define([nodes_copyright], ["2021 Barcelona Supercomputing Center (BSC)"])

AC_PREREQ([2.69])
AC_INIT([NODES], [nodes_version], [antoni.navarro@bsc.es])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AX_COMPILE_FLAGS

# Automake initialization
AM_INIT_AUTOMAKE([foreign -Wall dist-bzip2 -Wno-portability subdir-objects silent-rules])
AM_SILENT_RULES([yes])

# Checks for header files
AC_CHECK_HEADERS([unistd.h])

# AM_PROG_AR must be called before LT_INIT or a warning ensues
AM_PROG_AR
LT_INIT([shared disable-static pic-only])

# Checks for compiler characteristics
# Prepare compilation flags before AC_PROG_CXX
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17([noext], [mandatory])
AC_CHECK_MAIN_WRAPPER_TYPE

# Check for clang++
SSS_CHECK_NODES_CLANG

# Checks for libraries
AX_BOOST_BASE([1.71], [], [ AC_MSG_ERROR([Boost >= 1.71 is needed to build NODES]) ])
CHECK_PTHREAD
AC_CHECK_NOSV
AC_CHECK_OVNI

# Checks for typedefs, structures
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT

if test x"${ac_have_nodes_clang}" = x"no" ; then
	AC_MSG_WARN([the directive based tests will not be checked since there is no Clang compiler available])
fi
if test x"${ac_have_nosv}" = x"no" ; then
	AC_MSG_WARN([configured without nOS-V support])
fi

echo ""
echo ""
echo "Configuration summary:"
echo "    Compiler version...    ${CXX_VERSION}"
echo ""
echo "    Installation prefix... ${prefix}"
echo ""
echo "    CXXFLAGS...            ${nodes_CXXFLAGS} ${CXXFLAGS}"
echo "    CPPFLAGS...            ${nodes_CPPFLAGS} ${CPPFLAGS}"
echo ""
echo "    Code Model flags...    ${MCMODEL_FLAGS}"
echo ""
echo "    Boost CPPFLAGS...      ${BOOST_CPPFLAGS}"
echo "    Boost LDFLAGS...       ${BOOST_LDFLAGS}"
echo ""
echo "    nOS-V CPPFLAGS...      ${nosv_CPPFLAGS}"
echo ""
echo "    Ovni CPPFLAGS...       ${ovni_CPPFLAGS}"
echo "    Ovni LDFLAGS....       ${ovni_LIBS}"

