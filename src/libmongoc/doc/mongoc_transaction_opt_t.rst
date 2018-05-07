:man_page: mongoc_transaction_opt_t

mongoc_transaction_opt_t
========================

.. code-block:: c

  #include <mongoc.h>

  typedef struct _mongoc_transaction_opt_t mongoc_transaction_opt_t;

Synopsis
--------

Options for starting a multi-document transaction.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_transaction_opts_new
    mongoc_transaction_opts_get_read_concern
    mongoc_transaction_opts_set_read_concern
    mongoc_transaction_opts_get_write_concern
    mongoc_transaction_opts_set_write_concern
    mongoc_transaction_opts_get_read_prefs
    mongoc_transaction_opts_set_read_prefs
    mongoc_transaction_opts_clone
    mongoc_transaction_opts_destroy
