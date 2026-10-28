:man_page: mongoac_insert_many_result_t

mongoac_insert_many_result_t
============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_many_result-fwd.h>

   typedef struct mongoac_insert_many_result_t mongoac_insert_many_result_t;

   #include <mongoac/insert_many_result.h>

Description
-----------

Represents the result of an ``insertMany`` operation (`InsertManyResult <https://docs.rs/mongodb/latest/mongodb/results/struct.InsertManyResult.html>`_).

All ``mongoac_insert_many_result_t`` objects must be destroyed with :symbol:`mongoac_insert_many_result_destroy()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_insert_many_result_destroy
    mongoac_insert_many_result_clone

    mongoac_insert_many_result_get_inserted_ids
