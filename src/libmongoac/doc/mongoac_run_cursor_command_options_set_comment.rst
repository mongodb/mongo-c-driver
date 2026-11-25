:man_page: mongoac_run_cursor_command_options_set_comment

mongoac_run_cursor_command_options_set_comment()
================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_comment(mongoac_run_cursor_command_options_t *options,
                                                       mongoac_bson_view_t v,
                                                       mongoac_error_t *error);

Description
-----------

Sets the "comment" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``v`` (as a `Bson <https://docs.rs/bson/3.1.0/bson/enum.Bson.html>`_).

``v`` must be a BSON document of the form ``{"v": <value>}``, where ``<value>`` is the value used to set the comment field.

When ``options`` is null, does nothing and sets ``error``.

When ``v`` is null or invalid BSON, does nothing and sets ``error``.

When ``v`` has no ``"v"`` field, does nothing and sets ``error``.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``options`` is null.
- Invalid Argument (mongoac): ``v`` is null or invalid BSON.
- Invalid Argument (mongoac): ``v`` is missing the ``"v"`` field.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_comment`
   | :symbol:`mongoac_run_cursor_command_options_set_comment_none`
   | :symbol:`mongoac_run_cursor_command_options_set_comment_string`
