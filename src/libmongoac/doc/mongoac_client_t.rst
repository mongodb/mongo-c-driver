:man_page: mongoac_client_t

mongoac_client_t
================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client-fwd.h>

   typedef struct mongoac_client_t mongoac_client_t;

   #include <mongoac/client.h>

Description
-----------

Represents a `mongodb::Client <https://docs.rs/mongodb/latest/mongodb/struct.Client.html>`_ and its associated :symbol:`mongoac_runtime_t`.

All operations executed with a given client and its associated objects are spawned and driven with the associated runtime.
The associated runtime may be obtained with :symbol:`mongoac_client_get_runtime()`.

.. important::

  Calling :symbol:`mongoac_client_shutdown()` before :symbol:`mongoac_client_destroy()` is highly recommended to ensure a `clean shutdown <https://docs.rs/mongodb/latest/mongodb/struct.Client.html#clean-shutdown>`_.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_client_destroy
    mongoac_client_clone
    mongoac_client_new

    mongoac_client_get_runtime

    mongoac_client_shutdown
