:man_page: mongoac_run_cursor_command_options_set_max_time_none

mongoac_run_cursor_command_options_set_max_time_none()
======================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_max_time_none(mongoac_run_cursor_command_options_t *options);

Description
-----------

Sets the "max time" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``None``.

When ``options`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_max_time_is_none`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_from_millis`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_from_secs`
