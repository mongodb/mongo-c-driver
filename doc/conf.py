# -*- coding: utf-8 -*-
import os.path
import sys

# Ensure we can import "mongoc" and "taglist" extension modules.
sys.path.append(os.path.dirname(__file__))

extensions = [
    'mongoc',
    'taglist',
    'sphinx.ext.intersphinx',
]

# General information about the project.
project = 'MongoDB C Driver'
copyright = '2017, MongoDB, Inc'
author = 'MongoDB, Inc'

version_path = os.path.join(os.path.dirname(__file__), '..', 'VERSION_CURRENT')
version = open(version_path).read().strip()
release_path = os.path.join(os.path.dirname(__file__), '..', 'VERSION_RELEASED')
release = open(release_path).read().strip()
release_major, release_minor, release_patch = release.split('.')
rst_prolog = """
.. |release_major| replace:: %(release_major)s

.. |release_minor| replace:: %(release_minor)s

.. |release_patch| replace:: %(release_patch)s

.. |release_download| replace:: https://github.com/mongodb/mongo-c-driver/releases/download/%(release)s/mongo-c-driver-%(release)s.tar.gz
""" % locals()

language = 'en'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
master_doc = 'index'

# don't fetch libbson's inventory from mongoc.org during build - Debian and
# Fedora package builds must work offline - maintain a recent copy here
intersphinx_mapping = {
    'bson': ('http://mongoc.org/libbson/current', 'libbson-objects.inv'),
}

# -- Options for HTML output ----------------------------------------------

html_theme_path = ['.']
html_theme = 'mongoc-theme'
html_title = html_shorttitle = 'MongoDB C Driver %s' % version
# html_favicon = None
html_use_smartypants = False
html_sidebars = {
    '**': ['globaltoc.html'],
    'errors': [],  # Make more room for the big table.
}
html_show_sourcelink = False

# Note: http://www.sphinx-doc.org/en/1.5.1/config.html#confval-html_copy_source
# This will degrade the Javascript quicksearch if we ever use it.
html_copy_source = False

# -- Options for manual page output ---------------------------------------

# HACK: Just trick Sphinx's ManualPageBuilder into thinking there are pages
# configured - we'll do it dynamically in process_nodes.
man_pages = [True]

# If true, show URL addresses after external links.
#
# man_show_urls = False
from docutils.nodes import title


# To publish HTML docs at GitHub Pages, create .nojekyll file. In Sphinx 1.4 we
# could use the githubpages extension, but old Ubuntu still has Sphinx 1.3.
def create_nojekyll(app, env):
    if app.builder.format == 'html':
        path = os.path.join(app.builder.outdir, '.nojekyll')
        open(path, 'wt').close()


def setup(app):
    app.connect('doctree-read', process_nodes)
    app.connect('env-updated', create_nojekyll)


def process_nodes(app, doctree):
    if man_pages == [True]:
        man_pages.pop()

    env = app.env
    metadata = env.metadata[env.docname]

    # A page like installing.rst sets its name with ":man_page: mongoc_installing"
    page_name = metadata.get('man_page', env.docname)
    page_title = find_node(doctree, title)

    man_pages.append((env.docname, page_name, page_title.astext(), [author], 3))


def find_node(doctree, klass):
    matches = doctree.traverse(lambda node: isinstance(node, klass))
    if not matches:
        raise IndexError("No %s in %s" % (klass, doctree))

    return matches[0]
