#################################################
Building the |mongo-c-driver| Documentation Pages
#################################################

.. highlight:: console
.. default-role:: bash

This documentation is rendered using Sphinx__. To easily ensure that all tooling
matches expected versions, it is recommended to use Poetry__ to install and
run the required tools.

__ https://www.sphinx-doc.org
__ https://python-poetry.org

.. tip::

  Poetry itself may be installed externally, but can also be automatically
  managed using the included wrapping scripts for Bash (At ``tools/poetry.sh``)
  or PowerShell (at ``tools/poetry.ps1``). These scripts can stand in for
  ``poetry`` in any command written below.


Setting Up the Environment
**************************

To install the required tooling, use the `poetry install` command, enabling
documentation dependencies::

  $ poetry install --with=docs

This will create a user-local Python virtualenv that contains the necessary
tools for building this documentation. The `poetry install` command only needs
to be run when the `pyproject.toml` file is changed.


Running Sphinx
**************

Poetry can be used to execute the :std:doc:`man/sphinx-build` command::

  $ poetry run sphinx-build -b dirhtml "./src/libmongoc/doc" "./_build/docs/html"

This command will generate the HTML documentation in the `_build/docs/html`
subdirectory.

.. tip::

  Because Sphinx builds many pages, the build may run quite slowly. For faster
  builds, it is recommended to use the
  :std:option:`--jobs <sphinx:sphinx-build.--jobs>` command-line option when
  invoking :std:doc:`sphinx:man/sphinx-build`.


Viewing the Documentation
*************************

To quickly view the rendered HTML pages, a simple local HTTP server can be
spawned on the command line by using Python's built-in
:py:mod:`http.server` module:

.. code-block:: sh

  $ poetry run python -m http.server --directory=_build/docs/html

By default, this will serve the documentation at http://127.0.0.1:8000, which
you can open in any web browser to see the rendered pages.
