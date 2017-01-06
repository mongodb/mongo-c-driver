MAN_FILES_TO_BUILD = $(patsubst doc/%.page,doc/man/%.3,$(wildcard doc/*.page))
BUILT_MAN_FILES = $(wildcard doc/man/*.3)
CLEANFILES += $(wildcard doc/man/*.3)

dist_man_MANS = $(BUILT_MAN_FILES)
# Automake (at least up to 1.10) mishandles dist_man_MANS inside conditionals.
# Unlike with other dist primaries, the files are not distributed if the
# conditional is false.
# Work the bug around until it is fixed:
dist_noinst_DATA = $(dist_man_MANS)

man3_MANS = $(BUILT_MAN_FILES)

.PHONY: doc/man

doc/man:
	-mkdir -p doc/man
   # "E" is "ignore cache", "d" sets dir for cachefiles, "b" is build type.
	$(SPHINX_BUILD) -qEW -d /tmp/sphinx-cache-dir -b man doc doc/man

man: $(MAN_FILES_TO_BUILD)
EXTRA_DIST += $(MAN_FILES_TO_BUILD)
