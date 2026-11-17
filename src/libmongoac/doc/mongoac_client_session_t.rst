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

  A client session represents a set of *sequential* operations. Operations using the same session SHOULD NOT be executed
  concurrently. Each :symbol:`mongoac_future_t` representing an async operation associated with a given session must be
  :ref:`driven to completion <readiness>` before the next async operation associated with the same session is spawned.
  When two or more async operations associated with the same session are spawned concurrently, the behavior is
  unspecified.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_client_session_destroy
    mongoac_client_session_clone

    mongoac_client_session_get_id
