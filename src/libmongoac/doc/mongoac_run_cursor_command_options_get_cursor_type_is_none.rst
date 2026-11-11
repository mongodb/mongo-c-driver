:man_page: mongoac_run_cursor_command_options_get_cursor_type_is_none

mongoac_run_cursor_command_options_get_cursor_type_is_none()
============================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   bool mongoac_run_cursor_command_options_get_cursor_type_is_none(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns ``true`` when the "cursor type" field of the given :symbol:`mongoac_run_cursor_command_options_t` is ``None``.

When ``options`` is null, returns ``true``.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_cursor_type`
   | :symbol:`mongoac_run_cursor_command_options_set_cursor_type_none`
