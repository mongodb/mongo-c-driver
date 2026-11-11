:man_page: mongoac_run_cursor_command_options_set_comment_string

mongoac_run_cursor_command_options_set_comment_string()
=======================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_comment_string(mongoac_run_cursor_command_options_t *options,
                                                              mongoac_string_view_t v);

Description
-----------

Sets the "comment" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``v`` (as a `String <https://docs.rs/bson/3.1.0/bson/enum.Bson.html#variant.String>`_).

When ``options`` is null, does nothing.

When ``v`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_comment_string`
   | :symbol:`mongoac_run_cursor_command_options_set_comment`
   | :symbol:`mongoac_run_cursor_command_options_set_comment_none`
