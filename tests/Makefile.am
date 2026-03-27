AUTOMAKE_OPTIONS = subdir-objects

TESTS = \
		options_test \
		basic.sh \
		casual-overlay.sh \
		config-dir-hidden.sh \
		config-precedence.sh \
		home-cwd-guard.sh \
		jail-files.sh \
		job-control.sh \
		job-control-same-pgrp-stop.sh \
		job-control-sigtstp-fallback.sh \
		job-control-sigtstp-preserve.sh \
		modes.sh \
		mask.sh \
		process-state.sh \
		strict-home-grant.sh \
		sudo-env.sh \
		storage.sh \
		storage-from-conf.sh \
		teardown.sh

check_PROGRAMS = jai_test_probe jai_test_pty_driver options_test

jai_test_probe_SOURCES = jai-test-probe.cc
jai_test_pty_driver_SOURCES = jai-test-pty-driver.cc
options_test_SOURCES = options_test.cc ../options.cc ../options.h

EXTRA_DIST = $(TESTS) common.sh.in setup-setuid.sh.in

CLEANFILES = *~

maintainer-clean-local:
	-rm -rf .deps

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in

all-local:
	chmod +x $(builddir)/setup-setuid.sh
