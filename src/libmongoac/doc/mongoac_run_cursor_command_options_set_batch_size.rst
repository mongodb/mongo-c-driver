:man_page: mongoac_run_cursor_command_options_set_batch_size

mongoac_run_cursor_command_options_set_batch_size()
===================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   void mongoac_run_cursor_command_options_set_batch_size(mongoac_run_cursor_command_options_t *options,
                                                          uint32_t v);

Description
-----------

Sets the "batch size" field of the given :symbol:`mongoac_run_cursor_command_options_t` to ``v``.

When ``options`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_get_batch_size`
   | :symbol:`mongoac_run_cursor_command_options_set_batch_size_none`
