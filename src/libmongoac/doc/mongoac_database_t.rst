:man_page: mongoac_database_t

mongoac_database_t
==================

Synopsis
--------

.. code-block:: c

   #include <mongoac/database-fwd.h>

   typedef struct mongoac_database_t mongoac_database_t;

   #include <mongoac/database.h>

Description
-----------

Represents a `mongodb::Database <https://docs.rs/mongodb/latest/mongodb/struct.Database.html>`_.

All operations executed with a given database and its associated objects are spawned and driven with the associated runtime.
The associated runtime may be obtained with :symbol:`mongoac_database_get_runtime()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_database_destroy
    mongoac_database_clone

    mongoac_database_get_client
    mongoac_database_get_collection
    mongoac_database_get_name
    mongoac_database_get_runtime
    mongoac_database_get_runtime_address

    mongoac_database_create_collection
    mongoac_database_create_collection_async
    mongoac_database_drop
    mongoac_database_drop_async
    mongoac_database_run_command
    mongoac_database_run_command_async
