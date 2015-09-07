##  Twine: SPARQL client library
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014-2015 BBC
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

LDADD = @top_builddir@/libsparqlclient.la

dist_noinst_SCRIPTS = setup-4store.sh teardown-4store.sh

TESTS_ENVIRONMENT = eval `$(CONFIG_SHELL) ./setup-4store.sh` ;

EXTRA_DIST = setup-4store.sh.in teardown-4store.sh.in

DISTCLEANFILES = setup-4store.sh teardown-4store.sh

check-local: $(TEST_SUITE_LOG)
	$(CONFIG_SHELL) ./teardown-4store.sh

clean-local:
	$(CONFIG_SHELL) ./teardown-4store.sh

if RUN_TESTS

TESTS = 000-sanity

endif
