:man_page: mongoac_run_cursor_command_options_get_comment_string

mongoac_run_cursor_command_options_get_comment_string()
=======================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   mongoac_string_view_t mongoac_run_cursor_command_options_get_comment_string(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns the "comment" field of the given :symbol:`mongoac_run_cursor_command_options_t` as a :symbol:`mongoac_string_view_t`.

When the field is ``None``, or the value is not a BSON string, returns a null :symbol:`mongoac_string_view_t`.

When ``options`` is null, returns a null :symbol:`mongoac_string_view_t`.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_comment`
   | :symbol:`mongoac_run_cursor_command_options_get_comment_is_none`
   | :symbol:`mongoac_run_cursor_command_options_set_comment_string`
