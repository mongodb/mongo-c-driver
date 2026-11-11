:man_page: mongoac_run_cursor_command_options_get_max_time_subsec_millis

mongoac_run_cursor_command_options_get_max_time_subsec_millis()
===============================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   uint32_t mongoac_run_cursor_command_options_get_max_time_subsec_millis(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns the sub-second value (in milliseconds) of the "max time" field of the given :symbol:`mongoac_run_cursor_command_options_t`.

When the field is ``None``, returns ``0``.

When ``options`` is null, returns ``0``.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_max_time_as_secs`
   | :symbol:`mongoac_run_cursor_command_options_get_max_time_is_none`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_from_millis`
