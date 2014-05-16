BUILT_MAN_FILES = $(patsubst %.page,%.3,$(wildcard doc/mongoc_*.page))

%.3: %.page
	@ $(MKDIR_P) doc
	$(AM_V_GEN)./doc/mallard2man.py 3 $^

MAINTAINERCLEANFILES += $(BUILT_MAN_FILES)

man: $(BUILT_MAN_FILES)

html:
	@ $(MKDIR_P) doc/html
	$(AM_V_GEN)yelp-build html -o doc/html $(wildcard $(top_srcdir)/doc/*.page)

dist-hook: man html

dist_man_MANS = $(BUILT_MAN_FILES)

EXTRA_DIST += $(dist_man_MANS)

# Automake (at least up to 1.10) mishandles dist_man_MANS inside conditionals.
# Unlike with other dist primaries, the files are not distributed if the
# conditional is false.
# Work the bug around until it is fixed:
dist_noinst_DATA = $(dist_man_MANS)
