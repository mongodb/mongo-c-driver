:man_page: mongoac_run_cursor_command_options_set_max_time_from_secs

mongoac_run_cursor_command_options_set_max_time_from_secs()
===========================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_max_time_from_secs(mongoac_run_cursor_command_options_t *options,
                                                                  uint64_t v);

Description
-----------

Sets the "max time" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``v`` (in seconds).

When ``options`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_max_time_as_secs`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_from_millis`
   | :symbol:`mongoac_run_cursor_command_options_set_max_time_none`
