#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)

AC_DEFUN([AC_CHECK_MAIN_WRAPPER_TYPE],
	[
		AC_LANG_PUSH(C)

		AC_MSG_CHECKING([if target is PowerPC])
		AC_COMPILE_IFELSE(
			[ AC_LANG_SOURCE( [[
#ifndef __powerpc__
# error not power
#endif
]]
				) ],
			[ ac_target_is_powerpc=yes ],
			[ ac_target_is_powerpc=no ]
		)
		AC_MSG_RESULT([${ac_target_is_powerpc}])

		AC_MSG_CHECKING([if target is Linux])
		AC_COMPILE_IFELSE(
			[ AC_LANG_SOURCE( [[
#ifndef __linux__
# error not linux
#endif
]]
				) ],
			[ ac_target_is_linux=yes ],
			[ ac_target_is_linux=no ]
		)
		AC_MSG_RESULT([${ac_target_is_linux}])

		AC_LANG_POP(C)

		AM_CONDITIONAL([LINUX_POWERPC_GLIBC], [test "x${ac_target_is_linux}" = "xyes" && test "x${ac_target_is_powerpc}" = "xyes"])
		AM_CONDITIONAL([LINUX_GLIBC], [test "x${ac_target_is_linux}" = "xyes" && test "x${ac_target_is_powerpc}" = "xno"])
	]
)


