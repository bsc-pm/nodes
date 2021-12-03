#	This file is part of nODES and is licensed under the terms contained in the COPYING file.
#
#	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)


AC_DEFUN([SSS_CHECK_MERCURIUM],
	[
		AC_ARG_WITH(
			[mercurium],
			[AS_HELP_STRING([--with-mercurium=prefix], [specify the installation prefix of the Mercurium compiler, which must be compiled with Nanos6 support @<:@default=auto@:>@])],
			[ac_use_mercurium_prefix="${withval}"],
			[ac_use_mercurium_prefix="auto"]
		)

		AC_LANG_PUSH([C])
		AX_COMPILER_VENDOR
		AC_LANG_POP([C])

		AC_LANG_PUSH([C++])
		AX_COMPILER_VENDOR
		AC_LANG_POP([C++])

		if test "$ax_cv_c_compiler_vendor" = "intel" ; then
			mcc_vendor_prefix=i
		elif test "$ax_cv_c_compiler_vendor" = "ibm" ; then
			mcc_vendor_prefix=xl
		fi

		if test "$ax_cv_cxx_compiler_vendor" = "intel" ; then
			mcxx_vendor_prefix=i
		elif test "$ax_cv_cxx_compiler_vendor" = "ibm" ; then
			mcxx_vendor_prefix=xl
		fi

		if test x"${ac_use_mercurium_prefix}" = x"auto" || test x"${ac_use_mercurium_prefix}" = x"yes" ; then
			AC_PATH_PROGS(NODES_MCC, [${mcc_vendor_prefix}mcc.nanos6 ${mcc_vendor_prefix}mcc], [])
			AC_PATH_PROGS(NODES_MCXX, [${mcxx_vendor_prefix}mcxx.nanos6 ${mcc_vendor_prefix}mcxx], [])
			if test x"${NODES_MCC}" = x"" || test x"${NODES_MCXX}" = x"" ; then
				if test x"${ac_use_mercurium_prefix}" = x"yes"; then
					AC_MSG_ERROR([could not find nODES Mercurium])
				else
					AC_MSG_WARN([could not find nODES Mercurium])
					ac_have_mercurium=no
				fi
			else
				ac_use_mercurium_prefix=$(echo "${NODES_MCC}" | sed 's@/bin/'${mcc_vendor_prefix}'mcc.nanos6'\$'@@;s@/bin/'${mcc_vendor_prefix}'mcc'\$'@@')
				ac_have_mercurium=yes
			fi
		elif test x"${ac_use_mercurium_prefix}" != x"no" ; then
			AC_PATH_PROGS(NODES_MCC, [${mcc_vendor_prefix}mcc.nanos6 ${mcc_vendor_prefix}mcc], [], [${ac_use_mercurium_prefix}/bin])
			AC_PATH_PROGS(NODES_MCXX, [${mcxx_vendor_prefix}mcxx.nanos6 ${mcc_vendor_prefix}mcxx], [], [${ac_use_mercurium_prefix}/bin])
			if test x"${NODES_MCC}" = x"" || test x"${NODES_MCXX}" = x"" ; then
				AC_MSG_ERROR([could not find nODES Mercurium])
			else
				ac_use_mercurium_prefix=$(echo "${NODES_MCC}" | sed 's@/bin/'${mcc_vendor_prefix}'mcc.nanos6'\$'@@;s@/bin/'${mcc_vendor_prefix}'mcc'\$'@@')
				ac_have_mercurium=yes
			fi
		else
			ac_use_mercurium_prefix=""
			ac_have_mercurium=no
		fi

		AC_MSG_CHECKING([the nODES Mercurium installation prefix])
		if test x"${ac_have_mercurium}" = x"yes" ; then
			AC_MSG_RESULT([${ac_use_mercurium_prefix}])
		else
			AC_MSG_RESULT([not found])
		fi

		if test x"${NODES_MCC}" != x"" ; then
			ac_save_CC="${CC}"
			AC_LANG_PUSH(C)

			AC_MSG_CHECKING([which flag enables OmpSs-2 support in Mercurium])
			OMPSS2_FLAG=none

			mkdir -p conftest-header-dir/nanos6
			echo 'enum nanos6_multidimensional_dependencies_api_t { nanos6_multidimensional_dependencies_api = 2 };' > conftest-header-dir/nanos6/multidimensional-dependencies.h
			echo 'enum nanos6_multidimensional_release_api_t { nanos6_multidimensional_release_api = 1 };' > conftest-header-dir/nanos6/multidimensional-release.h

			# Try --ompss-v2
			CC="${NODES_MCC} --ompss-v2 -I${srcdir}/api -Iconftest-header-dir"
			AC_COMPILE_IFELSE(
				[ AC_LANG_SOURCE( [[
#ifndef __NODES__
#error Not nODES!
#endif

#ifndef __NANOS6__
#error Not Nanos6!
#endif

int main(int argc, char ** argv) {
	return 0;
}
]]
					) ],
				[ OMPSS2_FLAG=--ompss-v2 ],
				[ ]
			)

			# Try --ompss-2
			CC="${NODES_MCC} --ompss-2 -I${srcdir}/api -Iconftest-header-dir"
			AC_COMPILE_IFELSE(
				[ AC_LANG_SOURCE( [[
#ifndef __NODES__
#error Not nODES!
#endif

#ifndef __NANOS6__
#error Not Nanos6!
#endif

int main(int argc, char ** argv) {
	return 0;
}
]]
					) ],
				[ OMPSS2_FLAG=--ompss-2 ],
				[ ]
			)

			rm -Rf conftest-header-dir

			if test x"${OMPSS2_FLAG}" != x"none" ; then
				AC_MSG_RESULT([${OMPSS2_FLAG}])
				NODES_MCC="${NODES_MCC} ${OMPSS2_FLAG}"
				NODES_MCXX="${NODES_MCXX} ${OMPSS2_FLAG}"
			else
				AC_MSG_RESULT([none])
				AC_MSG_WARN([will not use ${NODES_MCC} since it does not support nODES])
				NODES_MCC=""
				NODES_MCXX=""
				OMPSS2_FLAG=""
				unset ac_use_mercurium_prefix
				ac_have_mercurium=no
			fi

			AC_LANG_POP(C)
			CC="${ac_save_CC}"

		fi

		NODES_MCC_PREFIX="${ac_use_mercurium_prefix}"
		AC_SUBST([NODES_MCC_PREFIX])
		AC_SUBST([NODES_MCC])
		AC_SUBST([NODES_MCXX])

		AM_CONDITIONAL(HAVE_NODES_MERCURIUM, test x"${ac_have_mercurium}" = x"yes")
	]
)

AC_DEFUN([SSS_PUSH_NODES_MERCURIUM],
	[
		AC_REQUIRE([SSS_CHECK_MERCURIUM])
		pre_nodes_cc="${CC}"
		pre_nodes_cxx="${CXX}"
		pre_nodes_cpp="${CPP}"
		pre_nodes_cxxcpp="${CXXCPP}"
		CC="${NODES_MCC} $1"
		CXX="${NODES_MCXX} $1"
		CPP="${NODES_MCC} -E $1"
		CXXPP="${NODES_MCXX} -E $1"
		AC_MSG_NOTICE([The following checks will be performed with Mercurium])
	]
)

AC_DEFUN([SSS_POP_NODES_MERCURIUM],
	[
		AC_MSG_NOTICE([The following checks will no longer be performed with Mercurium])
		CC="${pre_nodes_cc}"
		CXX="${pre_nodes_cxx}"
		CPP="${pre_nodes_cpp}"
		CXXPP="${pre_nodes_cxxcpp}"
	]
)

AC_DEFUN([SSS_REPLACE_WITH_MERCURIUM],
	[
		AC_MSG_NOTICE([Replacing the native compilers with Mercurium])
		NATIVE_CC="${CC}"
		CC="${MCC} --cc=$(echo ${NATIVE_CC} | awk '{ print '\$'1; }') --ld=$(echo ${NATIVE_CC} | awk '{ print '\$'1; }')"
		if test $(echo ${NATIVE_CC} | awk '{ print NF; }') -gt 1 ; then
			for extra_CC_param in $(echo ${NATIVE_CC} | cut -d " " -f 2-) ; do
				CC="${CC} --Wn,${extra_CC_param} --Wl,${extra_CC_param}"
			done
		fi

		NATIVE_CXX="${CXX}"
		CXX="${MCXX} --cxx=$(echo ${NATIVE_CXX} | awk '{ print '\$'1; }') --ld=$(echo ${NATIVE_CXX} | awk '{ print '\$'1; }')"
		if test $(echo ${NATIVE_CXX} | awk '{ print NF; }') -gt 1 ; then
			for extra_CXX_param in $(echo ${NATIVE_CXX} | cut -d " " -f 2-) ; do
				CXX="${CXX} --Wn,${extra_CXX_param} --Wl,${extra_CXX_param}"
			done
		fi

		NATIVE_CPP="${CPP}"
		CC="${CC} --cpp=$(echo ${NATIVE_CPP} | awk '{ print '\$'1; }')"
		CXX="${CXX} --cpp=$(echo ${NATIVE_CPP} | awk '{ print '\$'1; }')"
		if test $(echo ${NATIVE_CPP} | awk '{ print NF; }') -gt 1 ; then
			for extra_CPP_param in $(echo ${NATIVE_CPP} | cut -d " " -f 2-) ; do
				CC="${CC} --Wp,${extra_CPP_param}"
				CXX="${CXX} --Wp,${extra_CPP_param}"
			done
		fi

		AC_MSG_CHECKING([the Mercurium C compiler])
		AC_MSG_RESULT([${CC}])
		AC_MSG_CHECKING([the Mercurium C++ compiler])
		AC_MSG_RESULT([${CXX}])

		AC_SUBST([NATIVE_CC])
		AC_SUBST([NATIVE_CXX])
		AC_SUBST([NATIVE_CPP])

		if test x"${CC_VERSION}" != x"" ; then
			NATIVE_CC_VERSION="${CC_VERSION}"
			AC_SUBST([NATIVE_CC_VERSION])
			SSS_CHECK_CC_VERSION
		fi
		if test x"${CXX_VERSION}" != x"" ; then
			NATIVE_CXX_VERSION="${CXX_VERSION}"
			AC_SUBST([NATIVE_CXX_VERSION])
			SSS_CHECK_CXX_VERSION
		fi

		USING_MERCURIUM=yes
	]
)


AC_DEFUN([SSS_REPLACE_WITH_MERCURIUM_WRAPPER],
	[
		AC_MSG_NOTICE([Replacing the native compilers with a Mercurium wrapper])

		NATIVE_CPP="${CPP}"
		NATIVE_CC="${CC}"
		NATIVE_CXX="${CXX}"

		CC="${MCC}"
		CXX="${MCXX}"

		AC_MSG_CHECKING([the Mercurium C compiler])
		AC_MSG_RESULT([${CC}])
		AC_MSG_CHECKING([the Mercurium C++ compiler])
		AC_MSG_RESULT([${CXX}])

		AC_SUBST([NATIVE_CC])
		AC_SUBST([NATIVE_CXX])
		AC_SUBST([NATIVE_CPP])
		AC_SUBST([CC])
		AC_SUBST([CXX])

		if test x"${CC_VERSION}" != x"" ; then
			NATIVE_CC_VERSION="${CC_VERSION}"
			AC_SUBST([NATIVE_CC_VERSION])
			SSS_CHECK_CC_VERSION
		fi
		if test x"${CXX_VERSION}" != x"" ; then
			NATIVE_CXX_VERSION="${CXX_VERSION}"
			AC_SUBST([NATIVE_CXX_VERSION])
			SSS_CHECK_CXX_VERSION
		fi

		USING_MERCURIUM=yes
		USING_MERCURIUM_WRAPPER=yes

		AC_SUBST([NODES_MCC_CONFIG_DIR])
	]
)


AC_DEFUN([SSS_ALTERNATIVE_MERCURIUM_CONFIGURATION],
	[
		AC_MSG_CHECKING([the Mercurium configuration directory])
		MCC_CONFIG_DIR=$(${MCC} --print-config-dir | sed 's/.*: //')
		AC_MSG_RESULT([$MCC_CONFIG_DIR])
		AC_SUBST([MCC_CONFIG_DIR])

		AC_MSG_NOTICE([Creating local Mercurium configuration])
		mkdir -p mcc-config.d
		for config in $(cd "${MCC_CONFIG_DIR}"; eval 'echo *.config.*') ; do
			AC_MSG_NOTICE([Creating local Mercurium configuration file ${config}])
			# Replace the include directory and do not link automatically, since the runtime is compiled with libtool and has yet to be installed
			cat "${MCC_CONFIG_DIR}"/${config} | sed \
				's@{!nanox} linker_options = -L.*@{!nanox} linker_options = @;
				s@{!nanox,openmp}preprocessor_options = -I.*@{!nanox,openmp}preprocessor_options = -I'$(readlink -f "${srcdir}/../../src/api")' -include nanos6_rt_interface.h@;
				s@-lnanos6[[^ ]]*@@g;
				s@-Xlinker -rpath -Xlinker '"${MCC_PREFIX}/lib"'@@;
				s@-Xlinker -rpath -Xlinker '"${prefix}/lib"'@@' \
			> mcc-config.d/${config}
			LOCAL_MCC_CONFIG="${LOCAL_MCC_CONFIG} mcc-config.d/${config}"
		done
		AC_SUBST([LOCAL_MCC_CONFIG])

		if test x"${USING_MERCURIUM_WRAPPER}" != x"yes" ; then
			AC_MSG_CHECKING([how to select the local Mercurium configuration])
			ac_local_mercurium_profile_flags="--config-dir=${PWD}/mcc-config.d"
			AC_MSG_RESULT([${ac_local_mercurium_profile_flags}])

			CC="${CC} ${ac_local_mercurium_profile_flags} --profile=mcc"
			CXX="${CXX} ${ac_local_mercurium_profile_flags} --profile=mcxx"
		fi
	]
)


AC_DEFUN([SSS_CHECK_MERCURIUM_ACCEPTS_EXTERNAL_INSTALLATION],
	[
		if test x"${ac_have_mercurium}" = x"yes" ; then
			AC_MSG_CHECKING([if Mercurium allows using an external runtime])
			AC_LANG_PUSH([C])
			ac_save_[]_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS"
			_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS ${OMPSS2_FLAG} --no-default-nanos6-inc"
			AC_LINK_IFELSE(
				[AC_LANG_PROGRAM([[]], [[]])],
				[ac_mercurium_supports_external_installation=no],
				[ac_mercurium_supports_external_installation=yes]
			)
			AC_LANG_POP([C])
			_AC_LANG_PREFIX[]FLAGS="$ac_save_[]_AC_LANG_PREFIX[]FLAGS"
			AC_MSG_RESULT([$ac_mercurium_supports_external_installation])
		fi

		AM_CONDITIONAL([MCC_SUPORTS_EXTERNAL_INSTALL], [test x"${ac_mercurium_supports_external_installation}" = x"yes"])
	]
)

