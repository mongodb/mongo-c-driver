:man_page: mongoac_run_cursor_command_options_clone

mongoac_run_cursor_command_options_clone()
==========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   mongoac_run_cursor_command_options_t *mongoac_run_cursor_command_options_clone(const mongoac_run_cursor_command_options_t *options);

Description
-----------

Returns a new :symbol:`mongoac_run_cursor_command_options_t` with the same field values as ``options``.

When ``options`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_run_cursor_command_options_destroy()`.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_destroy`
   | :symbol:`mongoac_run_cursor_command_options_new`
