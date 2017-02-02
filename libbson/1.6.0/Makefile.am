CLEANFILES += $(wildcard doc/html/*.html)
CLEANFILES += $(wildcard doc/html/*.css)
CLEANFILES += $(wildcard doc/html/*.js)
CLEANFILES += $(wildcard doc/html/*.png)
CLEANFILES += doc/html/.nojekyll doc/html/objects.inv doc/html/searchindex.js

.PHONY: doc/html

# "E" is "ignore cache".
doc/html:
	-mkdir -p doc/html
	$(SPHINX_BUILD) -qEW $(top_srcdir)/doc doc/html

SPHINX_HTML_FILES = \
   doc/html/.nojekyll \
   doc/html/_static \
   doc/html/_static/ajax-loader.gif \
   doc/html/_static/basic.css \
   doc/html/_static/comment-bright.png \
   doc/html/_static/comment-close.png \
   doc/html/_static/comment.png \
   doc/html/_static/doctools.js \
   doc/html/_static/down-pressed.png \
   doc/html/_static/down.png \
   doc/html/_static/file.png \
   doc/html/_static/jquery.js \
   doc/html/_static/minus.png \
   doc/html/_static/mongoc.css \
   doc/html/_static/plus.png \
   doc/html/_static/pygments.css \
   doc/html/_static/searchtools.js \
   doc/html/_static/underscore.js \
   doc/html/_static/up-pressed.png \
   doc/html/_static/up.png \
   doc/html/_static/websupport.js \
   doc/html/objects.inv \
   doc/html/searchindex.js

EXTRA_DIST += doc/html
EXTRA_DIST += $(SPHINX_HTML_FILES)
