# -*- coding: utf-8 -*-
from docutils import nodes
import os

version = release = os.environ.get('VERSION_RELEASED')
if not release:
    raise RuntimeError('Set the "VERSION_RELEASED" environment variable')

# -- General configuration ------------------------------------------------
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'

# General information about the project.
project = u'mongoc.org'
copyright = u'2017, A. Jesse Jiryu Davis'
author = u'A. Jesse Jiryu Davis'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = False


# Support :download-link:`bson` or :download-link:`mongoc`.
def download_link(typ, rawtext, text, lineno, inliner, options={}, content=[]):
    if text == "mongoc":
        lib = "mongo-c-driver"
    elif text == "bson":
        lib = "libbson"
    else:
        raise ValueError(
            "download link must be mongoc or libbson, not \"%s\"" % text)

    title = "%s-%s.tar.gz" % (lib, version)
    url = ("https://github.com/mongodb/%(lib)s/"
           "releases/download/%(version)s/%(lib)s-%(version)s.tar.gz") % {
              "lib": lib,
              "version": version
          }

    pnode = nodes.reference(title, title, internal=False, refuri=url)
    return [pnode], []


def add_ga_javascript(app, pagename, templatename, context, doctree):
    context['metatags'] = context.get('metatags', '') + ''"""<script>
  (function(w,d,s,l,i){w[l]=w[l]||[];w[l].push(
      {'gtm.start': new Date().getTime(),event:'gtm.js'}
    );var f=d.getElementsByTagName(s)[0],
    j=d.createElement(s),dl=l!='dataLayer'?'&l='+l:'';j.async=true;j.src=
    '//www.googletagmanager.com/gtm.js?id='+i+dl;f.parentNode.insertBefore(j,f);
    })(window,document,'script','dataLayer','GTM-JQHP');</script>"""


def setup(app):
    app.add_role('download-link', download_link)
    app.connect('html-page-context', add_ga_javascript)

# -- Options for HTML output ----------------------------------------------

html_theme_path = ['.']
html_theme = 'mongoc-theme'
html_title = html_shorttitle = 'MongoDB C Driver %s' % version
# html_favicon = None
html_use_smartypants = False
html_show_sourcelink = False
html_use_index = False

# Note: http://www.sphinx-doc.org/en/1.5.1/config.html#confval-html_copy_source
# This will degrade the Javascript quicksearch if we ever use it.
html_copy_source = False
