:man_page: mongoc_installing
:orphan:

Installing the MongoDB C Driver (libmongoc) and BSON library (libbson)
======================================================================

.. note::

  Much of this section of documentation has been rewritten and moved:
  :doc:`/learn/get/index`

Supported Platforms
-------------------

.. note:: Moved: :doc:`/ref/platforms`


Install libmongoc with a Package Manager
----------------------------------------

.. note:: Moved: :doc:`/learn/get/installing`


.. _installing_libbson_with_pkg_manager:

Install libbson with a Package Manager
--------------------------------------

.. note:: Moved: :doc:`/learn/get/installing`

Docker image
------------

You can find a Docker image in `Docker Hub <https://hub.docker.com/r/mongodb/mongo-cxx-driver>`_
along with example usage of using libmongoc to ping a MongoDB database from
within a Docker container.

Build environment
-----------------

.. note:: Moved: :doc:`/learn/get/from-source`


Uninstalling the installed components
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There are two ways to uninstall the components that have been installed.  The first is to invoke the uninstall program directly.  On Linux/Unix:

.. code-block:: none

  $ sudo /usr/local/share/mongo-c-driver/uninstall.sh

On Windows:

.. code-block:: none

  $ C:\mongo-c-driver\share\mongo-c-driver\uninstall.bat

The second way to uninstall is from within the build directory, assuming that it is in the exact same state as when the install command was invoked:

.. code-block:: none

  $ sudo cmake --build . --target uninstall

The second approach simply invokes the uninstall program referenced in the first approach.

Dealing with Build Failures
^^^^^^^^^^^^^^^^^^^^^^^^^^^

If your attempt to build the C driver fails, please see the `README <https://github.com/mongodb/mongo-c-driver#how-to-ask-for-help>`_ for instructions on requesting assistance.

Additional Options for Integrators
----------------------------------

In the event that you are building the BSON library and/or the C driver to embed with other components and you wish to avoid the potential for collision with components installed from a standard build or from a distribution package manager, you can make use of the ``BSON_OUTPUT_BASENAME`` and ``MONGOC_OUTPUT_BASENAME`` options to ``cmake``.

.. code-block:: none

  $ cmake -DBSON_OUTPUT_BASENAME=custom_bson -DMONGOC_OUTPUT_BASENAME=custom_mongoc ..

The above command would produce libraries named ``libcustom_bson.so`` and ``libcustom_mongoc.so`` (or with the extension appropriate for the build platform).  Those libraries could be placed in a standard system directory or in an alternate location and could be linked to by specifying something like ``-lcustom_mongoc -lcustom_bson`` on the linker command line (possibly adjusting the specific flags to those required by your linker).
