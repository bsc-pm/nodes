#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)

AC_DEFUN([AX_CHECK_CXX_VERSION], [
	AC_MSG_CHECKING([the ${CXX} version])
	if test x"$CXX" != x"" ; then
		CXX_VERSION=$(${CXX} --version | head -1)
	fi
	AC_MSG_RESULT([$CXX_VERSION])
	AC_SUBST([CXX_VERSION])
])

AC_DEFUN([AX_COMPILE_FLAGS], [
	AC_ARG_ENABLE(
		[debug],
		[AS_HELP_STRING(
			[--enable-debug],
			[Adds compiler debug flags and enables additional internal debugging mechanisms @<:@default=disabled@:>@]
		)]
	)

	AS_IF([test "$enable_debug" = yes], [
		nodes_CPPFLAGS=""
		nodes_CXXFLAGS="-O0 -g3"
		nodes_CFLAGS="-O0 -g3"
	],[
		nodes_CPPFLAGS="-DNDEBUG"
		nodes_CXXFLAGS="-O3 -g"
		nodes_CFLAGS="-O3 -g"
	])

	AC_SUBST(nodes_CPPFLAGS)
	AC_SUBST(nodes_CXXFLAGS)
	AC_SUBST(nodes_CFLAGS)

	# Disable autoconf default compilation flags
	: ${CPPFLAGS=""}
	: ${CXXFLAGS=""}
	: ${CFLAGS=""}
])
