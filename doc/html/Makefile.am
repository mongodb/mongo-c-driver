CLEANFILES += $(wildcard doc/html/*.html)
CLEANFILES += $(wildcard doc/html/*.css)
CLEANFILES += $(wildcard doc/html/*.js)
CLEANFILES += $(wildcard doc/html/*.png)

.PHONY: doc/html

doc/html:
	-mkdir doc/html
	$(AM_V_GEN)$(YELP_BUILD) html -x $(top_srcdir)/build/libmongoc.xsl -o doc/html/ doc

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

EXTRA_DIST += doc/html
EXTRA_DIST += $(YELP_HTML_FILES)

