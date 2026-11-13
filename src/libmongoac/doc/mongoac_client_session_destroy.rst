:man_page: mongoac_client_session_destroy

mongoac_client_session_destroy()
================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client_session.h>

   void mongoac_client_session_destroy(mongoac_client_session_t *session);

Description
-----------

Destroy the given :symbol:`mongoac_client_session_t`.

When ``session`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_client_session_t`
