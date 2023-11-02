# -*- coding: utf-8 -*-
import http.client
import os.path
import sys
import urllib.request
from pathlib import Path
from typing import Any

from docutils.parsers.rst import directives
from sphinx.application import Sphinx
from sphinx.application import logger as sphinx_log
from sphinx.config import Config
from sphinx_design.dropdown import DropdownDirective

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
    "sphinx": ("https://www.sphinx-doc.org/en/master", "includes/sphinx.inv"),
    "python": ("https://docs.python.org/3", "includes/python.inv"),
    "bson": ("https://www.mongoc.org/libbson/current", "includes/libbson.inv"),
    "cmake": ("https://cmake.org/cmake/help/latest", "includes/cmake.inv"),
}

_UPDATE_KEY = "update_external_inventories"


def _maybe_update_inventories(app: Sphinx, config: Config):
    """
    We save Sphinx inventories for external projects saved within our own project
    so that we can support fully-offline builds. This is a convenience function
    to update those inventories automatically.

    This function will only have an effect if the appropriate command-line config
    value is defined.
    """
    prefix = "[libmongoc/doc/conf.py]"
    if not config[_UPDATE_KEY]:
        sphinx_log.info(
            "%s Using existing intersphinx inventories. Refresh by running with ‘-D %s=1’",
            prefix,
            _UPDATE_KEY,
        )
        return
    for name, tup in intersphinx_mapping.items():
        urlbase, filename = tup
        url = f"{urlbase}/objects.inv"
        sphinx_log.info("%s Downloading external inventory for %s from [%s]", prefix, name, url)
        with urllib.request.urlopen(url) as req:
            req: http.client.HTTPResponse = req
            dest = Path(app.srcdir) / filename
            sphinx_log.info("%s Saving inventory [%s] to file [%s]", prefix, url, dest)
            with dest.open("wb") as out:
                while buf := req.read(1024 * 4):
                    out.write(buf)
        sphinx_log.info(
            "%s Inventory file [%s] was updated. Commit the result to save it for subsequent builds.",
            prefix,
            dest,
        )


# -- Options for HTML output ----------------------------------------------

html_theme = "furo"
html_title = html_shorttitle = "libmongoc %s" % version
# html_favicon = None
html_use_index = True

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


class AdDropdown(DropdownDirective):
    """A sphinx-design dropdown that can also be an admonition."""

    option_spec = DropdownDirective.option_spec | {"admonition": directives.unchanged_required}

    def run(self):
        adm = self.options.get("admonition")
        if adm is not None:
            self.options.setdefault("class-container", []).extend(("admonition", adm))
            self.options.setdefault("class-title", []).append(f"admonition-title")
        return super().run()


def setup(app: Sphinx):
    mongoc_common_setup(app)
    app.add_directive("ad-dropdown", AdDropdown)
    app.connect("html-page-context", add_canonical_link)
    app.add_css_file("styles.css")
    app.connect("config-inited", _maybe_update_inventories)
    app.add_config_value(_UPDATE_KEY, default=False, rebuild=True, types=[bool])
