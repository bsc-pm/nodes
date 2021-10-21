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
SSS_PREPARE_COMPILER_FLAGS
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11([], [mandatory])

# Check for mercurium
SSS_CHECK_NANOS6_MERCURIUM
SSS_PUSH_NANOS6_MERCURIUM
SSS_CHECK_MERCURIUM_ACCEPTS_EXTERNAL_INSTALLATION
SSS_POP_NANOS6_MERCURIUM

# Checks for libraries
AX_BOOST_BASE([1.59], [], [ AC_MSG_ERROR([Boost library not found]) ])
CHECK_PTHREAD
AC_CHECK_NOSV

# Checks for typedefs, structures
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

if test x"${ac_have_nanos6_mercurium}" = x"yes" ; then
	NANOS6_MCXX="${NANOS6_MCXX} -std=c++03"
fi


SSS_FIXUP_COMPILER_FLAGS
AC_CHECK_EXTRACT_FIRST_COMPILER_FLAG([FALIGNED_NEW_FLAG], [-faligned-new])
AC_CHECK_EXTRACT_FIRST_COMPILER_FLAG([MCMODEL_FLAGS], [-mcmodel=large -mcmodel=medium])

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT

if test x"${ac_have_nanos6_mercurium}" = x"no" ; then
	AC_MSG_WARN([the directive based tests will not be checked since there is no Mercurium compiler available])
fi

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
