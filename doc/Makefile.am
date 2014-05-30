EXTRA_DIST += $(wildcard doc/*.page)

if ENABLE_MAN_PAGES
MAINTAINERCLEANFILES += $(BUILT_MAN_FILES)
BUILT_MAN_FILES = $(patsubst %.page,%.3,$(wildcard doc/mongoc_*.page))
EXTRA_DIST += $(BUILT_MAN_FILES)

dist_man_MANS = $(BUILT_MAN_FILES)
# Automake (at least up to 1.10) mishandles dist_man_MANS inside conditionals.
# Unlike with other dist primaries, the files are not distributed if the
# conditional is false.
# Work the bug around until it is fixed:
dist_noinst_DATA = $(dist_man_MANS)

man3_MANS = $(BUILT_MAN_FILES)

%.3: %.page
	@ $(MKDIR_P) doc
	$(AM_V_GEN)./doc/mallard2man.py 3 $^

man: $(BUILT_MAN_FILES)
endif

if ENABLE_HTML_DOCS
html:
	@ $(MKDIR_P) doc/html
	$(AM_V_GEN)$(YELP_BUILD) html -o doc/html $(wildcard $(top_srcdir)/doc/*.page)

EXTRA_DIST += $(wildcard doc/html/*)
DISTCLEANFILES += $(wildcard doc/html/*)
endif

dist-hook: man html
