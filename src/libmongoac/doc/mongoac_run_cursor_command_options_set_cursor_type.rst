:man_page: mongoac_run_cursor_command_options_set_cursor_type

mongoac_run_cursor_command_options_set_cursor_type()
====================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_cursor_type(mongoac_run_cursor_command_options_t *options,
                                                           mongoac_cursor_type_t v);

Description
-----------

Sets the "cursor type" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``v``.

Valid values for ``v`` are listed in :symbol:`mongoac_cursor_type_t`.

When ``options`` is null, does nothing.

When ``v`` is an unrecognized value, does nothing.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_cursor_type`
   | :symbol:`mongoac_run_cursor_command_options_set_cursor_type_none`
