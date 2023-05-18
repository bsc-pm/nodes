#	This file is part of NODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)

AC_DEFUN([AC_CHECK_NOSV],
	[
		AC_ARG_WITH(
			[nosv],
			[AS_HELP_STRING([--with-nosv=prefix], [specify the installation prefix of nOS-V])],
			[ ac_cv_use_nosv_prefix=$withval ],
			[ ac_cv_use_nosv_prefix="" ]
		)

		if test x"${ac_cv_use_nosv_prefix}" != x"" ; then
			if test x"${ac_cv_use_nosv_prefix}" = x"no" ; then
				AC_MSG_CHECKING([the nOS-V installation prefix])
				AC_MSG_RESULT([${ac_cv_use_nosv_prefix}])
				ac_use_nosv=no
			else
				AC_MSG_CHECKING([the nOS-V installation prefix])
				AC_MSG_RESULT([${ac_cv_use_nosv_prefix}])
				nosv_LIBS="-L${ac_cv_use_nosv_prefix}/lib -Wl,-rpath,${ac_cv_use_nosv_prefix}/lib -lnosv"
				nosv_CPPFLAGS="-I$ac_cv_use_nosv_prefix/include"
				ac_use_nosv=yes
			fi
		else
			AC_MSG_ERROR([nosv cannot be found, specify with --with-nosv])
		fi

		if test x"${ac_use_nosv}" != x"" ; then
			if test x"${ac_use_nosv}" = x"no" ; then
				AC_MSG_WARN([configuring without nosv.])
				ac_use_nosv=no
			else
				ac_save_CPPFLAGS="${CPPFLAGS}"
				ac_save_LIBS="${LIBS}"

				CPPFLAGS="${CPPFLAGS} ${nosv_CPPFLAGS}"
				LIBS="${LIBS} ${nosv_LIBS}"

				AC_CHECK_HEADERS([nosv.h])
				AC_CHECK_LIB([nosv],
					[nosv_init],
					[
						nosv_LIBS="${nosv_LIBS}"
						ac_use_nosv=yes
					],
					[
						if test x"${ac_cv_use_nosv_prefix}" != x"" ; then
							AC_MSG_ERROR([nosv cannot be found.])
						else
							AC_MSG_WARN([nosv cannot be found.])
						fi
						ac_use_nosv=no
					]
				)

				CPPFLAGS="${ac_save_CPPFLAGS}"
				LIBS="${ac_save_LIBS}"
			fi
		fi

		AC_SUBST([nosv_LIBS])
		AC_SUBST([nosv_CPPFLAGS])
	]
)
