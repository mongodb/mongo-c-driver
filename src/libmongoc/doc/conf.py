# -*- coding: utf-8 -*-
import os.path
import sys
from typing import Any

from sphinx.application import Sphinx

# Ensure we can import "mongoc" extension module.
this_path = os.path.dirname(__file__)
sys.path.append(this_path)
sys.path.append(os.path.normpath(os.path.join(this_path, "../../../build/sphinx")))

from mongoc_common import *

extensions = [
    "mongoc",
    "sphinx.ext.intersphinx",
]

# General information about the project.
project = "libmongoc"
copyright = "2017-present, MongoDB, Inc"
author = "MongoDB, Inc"

version_path = os.path.join(os.path.dirname(__file__), "../../..", "VERSION_CURRENT")
version = open(version_path).read().strip()

# The extension requires the "base" to contain '%s' exactly once, but we never intend to use it though

language = "en"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
master_doc = "index"

# Set an empty list of disabled reftypes.
# Sphinx 5.0 disables "std:doc" by default.
# Many documentation references use :doc:
intersphinx_disabled_reftypes = []

# don't fetch libbson's inventory from mongoc.org during build - Debian and
# Fedora package builds must work offline - maintain a recent copy here
intersphinx_mapping = {
    "bson": ("https://www.mongoc.org/libbson/current", "libbson-objects.inv"),
}

# -- Options for HTML output ----------------------------------------------

html_theme = "furo"
html_title = html_shorttitle = "libmongoc %s" % version
# html_favicon = None
html_use_index = False

rst_prolog = """
.. |qenc:is-experimental| replace::

    is part of the experimental
    :doc:`Queryable Encryption </queryable-encryption>` API and may be subject
    to breaking changes in future releases.

.. |qenc:opt-is-experimental| replace::

    This option |qenc:is-experimental|

.. |qenc:api-is-experimental| replace::

    This API |qenc:is-experimental|

.. |qenc:range-is-experimental| replace::

    Range algorithm is experimental only and not intended for public use. It is subject to breaking changes.

.. _the findAndModify command:
    https://www.mongodb.com/docs/manual/reference/command/findAndModify/

"""


def add_canonical_link(app: Sphinx, pagename: str, templatename: str, context: dict[str, Any], doctree: Any):
    link = f'<link rel="canonical" href="https://www.mongoc.org/libmongoc/current/{pagename}/"/>'

    context["metatags"] = context.get("metatags", "") + link


def setup(app):
    mongoc_common_setup(app)
    app.connect("html-page-context", add_canonical_link)
