:man_page: mongoac_insert_one_result_t

mongoac_insert_one_result_t
===========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_one_result-fwd.h>

   typedef struct mongoac_insert_one_result_t mongoac_insert_one_result_t;

   #include <mongoac/insert_one_result.h>

Description
-----------

Represents the result of an ``insertOne`` operation (`InsertOneResult <https://docs.rs/mongodb/latest/mongodb/results/struct.InsertOneResult.html>`_).

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_insert_one_result_destroy
    mongoac_insert_one_result_clone

    mongoac_insert_one_result_get_inserted_id
