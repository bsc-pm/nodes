#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)

AC_DEFUN([CHECK_PTHREAD],
	[
		AC_REQUIRE([AC_CANONICAL_HOST])
		AC_REQUIRE([AX_PTHREAD])

		# AX_PTHREAD does not seem to interact well with libtool on "regular" linux
		case $host_os in
			*linux*android*)
				;;
			*linux*)
				PTHREAD_LIBS="${PTHREAD_LIBS} -lpthread"
				;;
		esac
	]
)

