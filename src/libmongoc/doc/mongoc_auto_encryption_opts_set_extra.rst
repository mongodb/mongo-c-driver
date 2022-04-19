:man_page: mongoc_auto_encryption_opts_set_extra

mongoc_auto_encryption_opts_set_extra()
=======================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_extra (mongoc_auto_encryption_opts_t *opts,
                                          const bson_t *extra);


Parameters
----------

* ``opts``: The :symbol:`mongoc_auto_encryption_opts_t`
* ``extra``: A :symbol:`bson_t` of additional options.

``extra`` is a :symbol:`bson_t` containing any of the following optional fields:

* ``mongocryptdURI`` set to a URI to connect to the mongocryptd process (default is "mongodb://localhost:27020").
* ``mongocryptdBypassSpawn`` set to true to prevent the driver from spawning the mongocryptd process (default behavior is to spawn).
* ``mongocryptdSpawnPath`` set to a path (with trailing slash) to search for mongocryptd (defaults to empty string and uses default system paths).
* ``mongocryptdSpawnArgs`` set to an array of string arguments to pass to ``mongocryptd`` when spawning (defaults to ``[ "--idleShutdownTimeoutSecs=60" ]``).
* ``csflePath`` - Set a filepath string referring to a ``csfle`` dynamic library
  file. Default is undefined.

  * If not specified (the default), ``libmongocrypt`` will attempt to load
    ``csfle`` using the host system's default dynamic-library-search system.
  * If used, should be the path to the ``csfle`` dynamic library file itself,
    not just the directory that contains it.
  * If the given path is a relative path and the first path component is
    ``$ORIGIN``, the ``$ORIGIN`` component will be replaced with the absolute
    path to the directory containing the ``libmongocrypt`` library in use by the
    application.

    .. note:: No other ``RPATH``/``RUNPATH``-style substitutions are availbale.

  * If the given path is a relative path, the path will be resolves relative to
    the working directory of the operating system process.
  * If this option is specified and ``libmongocrypt`` fails to load ``csfle``
    from that filepath, ``libmongocrypt`` will fail to initialize with a
    hard-error, and will not attempt to search for ``csfle`` in any other
    locations.

* ``csfleRequired`` - If set to ``true``, and ``libmongocrypt`` fails to load a
  ``csfle`` dynamic library, generate a hard-error and do not attempt to spawn
  ``mongocryptd``.

  If set to ``false`` (the default), ``csflePath`` is not specified, *and*
  ``libmongocrypt`` fails to load ``csfle``, then ``libmongocrypt`` will proceed
  without ``csfle`` and fall back to using ``mongocryptd``.

For more information, see the `Client-Side Encryption specification <https://github.com/mongodb/specifications/blob/master/source/client-side-encryption/client-side-encryption.rst#extraoptions>`_.

.. seealso::

  | :symbol:`mongoc_client_enable_auto_encryption()`

  | The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`

