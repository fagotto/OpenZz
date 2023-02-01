## OpenZz gnu build process scripts.  
## If the 'configure' file is missing
##   run the "autogen.sh" script.

EXTRA_DIST = BUGS ChangeLog autogen.sh openzz.spec
SUBDIRS = src doc testsuite

LIBTOOL_DEPS = @LIBTOOL_DEPS@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool
