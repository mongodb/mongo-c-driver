# -*- coding: utf-8 -*-
import os.path
import sys

# Ensure we can import "taglist" extension module.
this_path = os.path.dirname(__file__)
sys.path.append(os.path.normpath(os.path.join(this_path, '../../../build/sphinx')))

from mongoc_common import *

extensions = [
    'mongoc',
    'taglist',
    'sphinx.ext.extlinks',
]

# General information about the project.
project = 'Libbson'
copyright = '2017-present, MongoDB, Inc'

version_path = os.path.join(
    os.path.dirname(__file__), '../../..', 'VERSION_CURRENT')
version = open(version_path).read().strip()
release_path = os.path.join(
    os.path.dirname(__file__), '../../..', 'VERSION_RELEASED')
release = open(release_path).read().strip()
release_major, release_minor, release_patch = release.split('.')
release_download = 'https://github.com/mongodb/libbson/releases/download/{0}/libbson-{0}.tar.gz'.format(release)
rst_prolog = """
.. |release_major| replace:: %(release_major)s

.. |release_minor| replace:: %(release_minor)s

.. |release_patch| replace:: %(release_patch)s

.. |release_download| replace:: https://github.com/mongodb/libbson/releases/download/%(release)s/libbson-%(release)s.tar.gz
""" % locals()

# The extension requires the "base" to contain '%s' exactly once, but we never intend to use it though
extlinks = {'release': (release_download+'%s', '')}

language = 'en'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
master_doc = 'index'

# -- Options for HTML output ----------------------------------------------

html_theme_path = ['../../../build/sphinx']
html_theme = 'mongoc-theme'
html_title = html_shorttitle = 'libbson %s' % version
# html_favicon = None

html_sidebars = {
    '**': ['globaltoc.html'],
    'errors': [],  # Make more room for the big table.
}


def add_canonical_link(app, pagename, templatename, context, doctree):
    link = ('<link rel="canonical"'
            ' href="http://mongoc.org/libbson/current/%s.html"/>' % pagename)

    context['metatags'] = context.get('metatags', '') + link


def setup(app):
    mongoc_common_setup(app)
    app.connect('html-page-context', add_canonical_link)
