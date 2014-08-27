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
html $(YELP_HTML_FILES):
	@ $(MKDIR_P) doc/html
	$(AM_V_GEN)$(YELP_BUILD) html -o doc/html $(wildcard $(top_srcdir)/doc/*.page)

YELP_HTML_FILES = \
	doc/html/C.css \
	doc/html/jquery.js \
	doc/html/jquery.syntax.brush.clang.js \
	doc/html/jquery.syntax.core.js \
	doc/html/jquery.syntax.js \
	doc/html/jquery.syntax.layout.yelp.js \
	doc/html/yelp-note-tip.png \
	doc/html/yelp-note-warning.png \
	doc/html/yelp-note.png \
	doc/html/yelp.js

BUILT_HTML_FILES = $(patsubst doc/%.page,doc/html/%.html,$(wildcard doc/*.page))
EXTRA_DIST += $(BUILT_HTML_FILES)
EXTRA_DIST += $(YELP_HTML_FILES)
DISTCLEANFILES += $(wildcard doc/html/*)
endif

dist-hook: man html
