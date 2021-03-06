## SPARQL client library
##
## Author: Mo McRoberts
##
## Copyright (c) 2014-2017 BBC
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

ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = m4 libedit t

SUBDIRS = @subdirs@ . t

pkgconfigdir = @libdir@/pkgconfig

LIBS =

AM_CPPFLAGS = @AM_CPPFLAGS@

include_HEADERS = libsparqlclient.h

lib_LTLIBRARIES = libsparqlclient.la

libsparqlclient_la_SOURCES = p_libsparqlclient.h libsparqlclient.h \
	connection.c update.c query.c query-model.c datastore-put.c \
	perform-query.c resultset.c urlencode.c vasprintf.c curl.c \
	datastore-post.c

libsparqlclient_la_LDFLAGS = -avoid-version

libsparqlclient_la_LIBADD = @LIBCURL_LOCAL_LIBS@ @LIBCURL_LIBS@ \
	@LIBXML2_LOCAL_LIBS@ @LIBXML2_LIBS@ \
	@LIBRAPTOR2_LOCAL_LIBS@ @LIBRAPTOR2_LIBS@ \
	@LIBRDF_LOCAL_LIBS@ @LIBRDF_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	@PTHREAD_LOCAL_LIBS@ @PTHREAD_LIBS@

bin_PROGRAMS = libsparqlclient-config sparql-query isparql

libsparqlclient_config_SOURCES = libsparqlclient-config.c

pkgconfig_DATA = libsparqlclient.pc

EXTRA_DIST = .release LICENSE-2.0

dist_man3_MANS = libsparqlclient.3

libsparqlclient.3: $(srcdir)/libsparqlclient.xml
	$(XML2MAN) $<

sparql_query_SOURCES = sparql-query.c
sparql_query_LDADD = libsparqlclient.la \
	@LIBRDF_LOCAL_LIBS@ @LIBRDF_LIBS@ \
	@LIBRAPTOR2_LOCAL_LIBS@ @LIBRAPTOR2_LIBS@

isparql_SOURCES = isparql.c
isparql_LDADD = libsparqlclient.la \
	@LIBRDF_LOCAL_LIBS@ @LIBRDF_LIBS@ \
	@LIBRAPTOR2_LOCAL_LIBS@ @LIBRAPTOR2_LIBS@ \
	@LIBEDIT_LOCAL_LIBS@ @LIBEDIT_LIBS@

BRANCH ?= develop

DEVELOP_SUBMODULES = m4

checkout:
	@for submod in $(DEVELOP_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(BRANCH) && \
		git pull && \
		$(MAKE) checkout BRANCH=$(BRANCH) || exit $? && \
		cd .. ; \
	done
