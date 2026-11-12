:man_page: mongoac_run_cursor_command_options_get_cursor_type

mongoac_run_cursor_command_options_get_cursor_type()
====================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   mongoac_cursor_type_t mongoac_run_cursor_command_options_get_cursor_type(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns the "cursor type" field of the given :symbol:`mongoac_run_cursor_command_options_t`.

When the field is ``None``, returns ``MONGOAC_CURSOR_TYPE_UNKNOWN``.

When ``options`` is null, returns ``MONGOAC_CURSOR_TYPE_UNKNOWN``.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_cursor_type_is_none`
   | :symbol:`mongoac_run_cursor_command_options_set_cursor_type`
