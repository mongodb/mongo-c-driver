:man_page: mongoac_client_session_t

mongoac_client_session_t
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client_session-fwd.h>

   typedef struct mongoac_client_session_t mongoac_client_session_t;

   #include <mongoac/client_session.h>

Description
-----------

Represents a `mongodb::ClientSession <https://docs.rs/mongodb/latest/mongodb/struct.ClientSession.html>`_.

.. important::

  A client session may only be used by a single thread at a time!

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_client_session_destroy
    mongoac_client_session_clone

    mongoac_client_session_get_id
