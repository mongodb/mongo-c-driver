:man_page: mongoac_run_cursor_command_options_get_batch_size

mongoac_run_cursor_command_options_get_batch_size()
===================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   uint32_t mongoac_run_cursor_command_options_get_batch_size(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns the value of the "batch size" field of the given :symbol:`mongoac_run_cursor_command_options_t`.

When the field is ``None``, returns ``0``.

When ``options`` is null, returns ``0``.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_batch_size_is_none`
   | :symbol:`mongoac_run_cursor_command_options_set_batch_size`
