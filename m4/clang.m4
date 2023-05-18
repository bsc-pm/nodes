#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2020-2023 Barcelona Supercomputing Center (BSC)

AC_DEFUN([SSS_CHECK_NODES_CLANG],
	[
		AC_ARG_WITH(
			[nodes-clang],
			[AS_HELP_STRING([--with-nodes-clang=prefix], [specify the installation prefix of the NODES Clang compiler @<:@default=auto@:>@])],
			[ac_use_nodes_clang_prefix="${withval}"],
			[ac_use_nodes_clang_prefix="auto"]
		)

		if test x"${ac_use_nodes_clang_prefix}" = x"auto" || test x"${ac_use_nodes_clang_prefix}" = x"yes" ; then
			AC_PATH_PROGS(NODES_CLANG, [clang], [])
			AC_PATH_PROGS(NODES_CLANGXX, [clang++], [])
			if test x"${NODES_CLANG}" = x"" || test x"${NODES_CLANGXX}" = x"" ; then
				if test x"${ac_use_nodes_clang_prefix}" = x"yes"; then
					AC_MSG_ERROR([could not find NODES Clang])
				else
					AC_MSG_WARN([could not find NODES Clang])
					ac_have_nodes_clang=no
				fi
			else
				ac_use_nodes_clang_prefix=$(echo "${NODES_CLANG}" | sed 's@/bin/clang@@')
				ac_have_nodes_clang=yes
			fi
		elif test x"${ac_use_nodes_clang_prefix}" != x"no" ; then
			AC_PATH_PROGS(NODES_CLANG, [clang], [], [${ac_use_nodes_clang_prefix}/bin])
			AC_PATH_PROGS(NODES_CLANGXX, [clang++], [], [${ac_use_nodes_clang_prefix}/bin])
			if test x"${NODES_CLANG}" = x"" || test x"${NODES_CLANGXX}" = x"" ; then
				AC_MSG_ERROR([could not find NODES Clang])
			else
				ac_use_nodes_clang_prefix=$(echo "${NODES_CLANG}" | sed 's@/bin/clang@@')
				ac_have_nodes_clang=yes
			fi
		else
			ac_use_nodes_clang_prefix=""
			ac_have_nodes_clang=no
		fi

		AC_MSG_CHECKING([the NODES Clang installation prefix])
		if test x"${ac_have_nodes_clang}" = x"yes" ; then
			AC_MSG_RESULT([${ac_use_nodes_clang_prefix}])
		else
			AC_MSG_RESULT([not found])
		fi

		if test x"${NODES_CLANG}" != x"" ; then
			ac_save_CC="${CC}"
			AC_LANG_PUSH(C)

			AC_MSG_CHECKING([which flag enables OmpSs-2 support in Clang])
			OMPSS2_FLAG=none

			CC="${NODES_CLANG} -fompss-2=libnodes"
			AC_COMPILE_IFELSE(
				[ AC_LANG_SOURCE( [[
int main(int argc, char ** argv) {
	return 0;
}
]]
					) ],
				[ OMPSS2_FLAG=-fompss-2=libnodes ],
				[ ]
			)

			if test x"${OMPSS2_FLAG}" != x"none" ; then
				AC_MSG_RESULT([${OMPSS2_FLAG}])
				NODES_CLANG="${NODES_CLANG} ${OMPSS2_FLAG} --gcc-toolchain=\$(subst bin/gcc,,\$(shell which gcc))"
				NODES_CLANGXX="${NODES_CLANGXX} ${OMPSS2_FLAG} --gcc-toolchain=\$(subst bin/g++,,\$(shell which g++))"
			else
				AC_MSG_RESULT([none])
				AC_MSG_WARN([will not use ${NODES_CLANG} since it does not support NODES])
				NODES_CLANG=""
				NODES_CLANGXX=""
				OMPSS2_FLAG=""
				unset ac_use_nodes_clang_prefix
				ac_have_nodes_clang=no
			fi

			AC_LANG_POP(C)
			CC="${ac_save_CC}"

		fi

		NODES_CLANG_PREFIX="${ac_use_nodes_clang_prefix}"
		AC_SUBST([NODES_CLANG_PREFIX])
		AC_SUBST([NODES_CLANG])
		AC_SUBST([NODES_CLANGXX])

		AM_CONDITIONAL(HAVE_NODES_CLANG, test x"${ac_have_nodes_clang}" = x"yes")
	]
)
