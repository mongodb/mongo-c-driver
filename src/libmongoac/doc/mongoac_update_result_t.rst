:man_page: mongoac_update_result_t

mongoac_update_result_t
=======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/update_result-fwd.h>

   typedef struct mongoac_update_result_t mongoac_update_result_t;

   #include <mongoac/update_result.h>

Description
-----------

Represents the result of an ``updateOne``, ``updateMany``, or ``replaceOne`` operation (`UpdateResult <https://docs.rs/mongodb/latest/mongodb/results/struct.UpdateResult.html>`_).

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_update_result_destroy
    mongoac_update_result_clone

    mongoac_update_result_get_matched_count
    mongoac_update_result_get_modified_count
    mongoac_update_result_get_upserted_id
