:man_page: mongoac_client_session_get_id

mongoac_client_session_get_id()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/client_session.h>

   mongoac_bson_t mongoac_client_session_get_id(const mongoac_client_session_t *session,
                                                mongoac_error_t *error);

Description
-----------

Returns the id of the given :symbol:`mongoac_client_session_t` as a BSON document of the form ``{"id": <UUID>}``.

The returned :symbol:`mongoac_bson_t` must be destroyed with :symbol:`mongoac_bson_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``session`` is null.
- Client-Side Error (bson): the session id could not be serialized.

.. seealso::

   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
