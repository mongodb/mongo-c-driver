EXTRA_DIST += $(wildcard doc/*.rst)

# TODO: remove libbson-objects.inv once libbson's Sphinx docs are on mongoc.org
EXTRA_DIST += \
  doc/conf.py \
  doc/githubpages.py \
  doc/libbson-objects.inv \
  doc/mongoc/__init__.py \
  doc/mongoc-theme/globaltoc.html \
  doc/mongoc-theme/layout.html \
  doc/mongoc-theme/static/mongoc.css_t \
  doc/mongoc-theme/static/pygments.css \
  doc/mongoc-theme/theme.conf \
  doc/taglist/__init__.py

dist-hook: man html

clean-local:
	-rm -rf doc/man/.buildinfo doc/man/.doctrees
	-rm -rf doc/html/.buildinfo doc/html/.doctrees doc/html/_static
