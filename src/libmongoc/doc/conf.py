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
    # NOTE: We use our own "minimal" CMake domain that lets us refer to external
    # objects from the CMake inventory, but provides no other features. The
    # build *could* otherwise use sphinxcontrib-moderncmakedomain, which is
    # more full-featured, but it is not (currently) available in repositories for
    # package building.
    # "sphinxcontrib.moderncmakedomain",
    "cmakerefdomain",
    "sphinx_design",
    "sphinx.ext.mathjax",
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
html_static_path = ["static"]

# Set an empty list of disabled reftypes.
# Sphinx 5.0 disables "std:doc" by default.
# Many documentation references use :doc:
intersphinx_disabled_reftypes = []
# Give the peer 30sec to give us the inventory:
intersphinx_timeout = 30

intersphinx_mapping = {
    # don't fetch libbson's inventory from mongoc.org during build - Debian and
    # Fedora package builds must work offline - maintain a recent copy here
    "bson": ("https://www.mongoc.org/libbson/current", "libbson-objects.inv"),
    # NOTE: Using a CMake version here, but is safe to update as new CMake is released.
    # This pin version ensures that documentation labels remain stable between builds.
    # When updating, fix any moved/renamed references as applicable.
    "cmake": ("https://cmake.org/cmake/help/v3.27", None),
    "sphinx": ("https://www.sphinx-doc.org/en/master", None),
    "python": ("https://docs.python.org/3", None),
}

# -- Options for HTML output ----------------------------------------------

html_theme = "furo"
html_title = html_shorttitle = "libmongoc %s" % version
# html_favicon = None
html_use_index = False

rst_prolog = rf"""
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

.. |version| replace:: {version}
.. |version.pre| replace:: ``{version}``
.. |vversion| replace:: ``v{version}``

.. role:: bash(code)
    :language: bash

.. role:: batch(code)
    :language: batch

.. role:: c(code)
    :language: c

.. role:: cpp(code)
    :language: c++

.. role:: bolded-name(literal)
    :class: bolded-name

.. |libbson| replace:: :bolded-name:`libbson`
.. |libmongoc| replace:: :bolded-name:`libmongoc`
.. |mongo-c-driver| replace:: :bolded-name:`mongo-c-driver`

.. The CMake inventory mangles the names of its custom domain objects, for some reason?
   Offer these substitutions for simpler variable references:

.. |cmvar:CMAKE_BUILD_TYPE| replace::
    :external+cmake:cmake:variable:`CMAKE_BUILD_TYPE <variable:CMAKE_BUILD_TYPE>`

.. |cmvar:CMAKE_INSTALL_PREFIX| replace::
    :external+cmake:cmake:variable:`CMAKE_INSTALL_PREFIX <variable:CMAKE_INSTALL_PREFIX>`

.. |cmvar:CMAKE_PREFIX_PATH| replace::
    :external+cmake:cmake:variable:`CMAKE_PREFIX_PATH <variable:CMAKE_PREFIX_PATH>`

.. |cmcmd:find_package| replace::
    :external+cmake:cmake:command:`find_package() <command:find_package>`

"""


def add_canonical_link(app: Sphinx, pagename: str, templatename: str, context: dict[str, Any], doctree: Any):
    link = f'<link rel="canonical" href="https://www.mongoc.org/libmongoc/current/{pagename}/"/>'

    context["metatags"] = context.get("metatags", "") + link


def setup(app: Sphinx):
    mongoc_common_setup(app)
    app.connect("html-page-context", add_canonical_link)
    app.add_css_file("styles.css")
