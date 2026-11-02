:man_page: mongoac_delete_result_t

mongoac_delete_result_t
=======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/delete_result-fwd.h>

   typedef struct mongoac_delete_result_t mongoac_delete_result_t;

   #include <mongoac/delete_result.h>

Description
-----------

Represents the result of a ``deleteOne`` operation (`DeleteResult <https://docs.rs/mongodb/latest/mongodb/results/struct.DeleteResult.html>`_).

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_delete_result_destroy
    mongoac_delete_result_clone

    mongoac_delete_result_get_deleted_count
