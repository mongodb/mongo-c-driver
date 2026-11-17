:man_page: mongoac_client_session_clone

mongoac_client_session_clone()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client_session.h>

   mongoac_client_session_t *mongoac_client_session_clone(const mongoac_client_session_t *session);

Description
-----------

Returns a new :symbol:`mongoac_client_session_t` with the same shared state as ``session``.

When ``session`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_client_session_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. important::

  A client session may only be used by a single thread at a time!

.. seealso::

   | :symbol:`mongoac_client_session_destroy`
   | :symbol:`mongoac_client_session_get_id`
