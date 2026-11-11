:man_page: mongoac_run_cursor_command_options_get_comment

mongoac_run_cursor_command_options_get_comment()
================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   mongoac_bson_t mongoac_run_cursor_command_options_get_comment(const mongoac_run_cursor_command_options_t *options,
                                                                 mongoac_error_t *error);

Description
-----------

Returns the "comment" field of the given :symbol:`mongoac_run_cursor_command_options_t` as an owning :symbol:`mongoac_bson_t` of the form ``{"v": <value>}``.

When the field is ``None``, returns a null :symbol:`mongoac_bson_t`.

When ``options`` is null, returns a null :symbol:`mongoac_bson_t`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``options`` is null.
- Client-Side Error (bson): the "comment" field could not be represented as a :symbol:`mongoac_bson_t`.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_comment_is_none`
   | :symbol:`mongoac_run_cursor_command_options_get_comment_string`
   | :symbol:`mongoac_run_cursor_command_options_set_comment`
