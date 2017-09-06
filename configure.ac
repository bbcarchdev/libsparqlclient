##  SPARQL client library
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014-2017 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

AC_INIT([libsparqlclient],m4_esyscmd([/bin/sh m4/get-version.sh]))
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC([clang c99 gcc cc])
BT_PROG_CC_DEBUG
BT_PROG_CC_WARN

LT_INIT

BT_ENABLE_POSIX_FULL
AC_SYS_LARGEFILE

BT_REQUIRE_PTHREAD
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBURI

BT_BUILD_DOCS

BT_DEFINE_PREFIX

dnl libedit is used only by isparql, don't include it in $LIBS
BT_REQUIRE_LIBEDIT_INCLUDED([
AM_CPPFLAGS="$AM_CPPFLAGS $LIBEDIT_CPPFLAGS"
AM_LDFLAGS="$AM_LDFLAGS $LIBEDIT_LDFLAGS"
])

dnl Check for python and 4store in order to run tests
run_tests=yes
AC_CHECK_PROGS([PYTHON],[python python2 python3],[])
AC_CHECK_PROGS([FOURSTORE_BACKEND_SETUP],[4s-backend-setup],[])
AC_CHECK_PROGS([FOURSTORE_HTTPD],[4s-httpd],[])
AC_CHECK_PROGS([FOURSTORE_ADMIN],[4s-admin],[])
AC_CHECK_PROGS([FOURSTORE_BOSS],[4s-boss],[])

AC_MSG_CHECKING([whether to enable the test-suite])
test x"$PYTHON" = x"" && run_tests=no
test x"$FOURSTORE_BACKEND_SETUP" = x"" && run_tests=no
test x"$FOURSTORE_HTTPD" = x"" && run_tests=no
test x"$FOURSTORE_ADMIN" = x"" && run_tests=no
test x"$FOURSTORE_BOSS" = x"" && run_tests=no
AC_MSG_RESULT([$run_tests])
AM_CONDITIONAL([RUN_TESTS],[test $run_tests = yes])

AC_CONFIG_FILES([
Makefile
libsparqlclient.pc
libsparqlclient-uninstalled.pc
t/Makefile
m4/Makefile
])

AC_CONFIG_FILES([t/setup-4store.sh],[chmod +x t/setup-4store.sh])
AC_CONFIG_FILES([t/teardown-4store.sh],[chmod +x t/teardown-4store.sh])

AC_OUTPUT
