:man_page: mongoac_run_cursor_command_options_get_max_time_as_secs

mongoac_run_cursor_command_options_get_max_time_as_secs()
=========================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   uint64_t mongoac_run_cursor_command_options_get_max_time_as_secs(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns the value (in seconds) of the "max time" field of the given :symbol:`mongoac_run_cursor_command_options_t`.

When the field is ``None``, returns ``0``.

When ``options`` is null, returns ``0``.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_max_time_is_none`
   | :symbol:`mongoac_run_cursor_command_options_get_max_time_subsec_millis`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_from_secs`
