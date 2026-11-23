:man_page: mongoac_run_cursor_command_options_new

mongoac_run_cursor_command_options_new()
========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options.h>

   mongoac_run_cursor_command_options_t *mongoac_run_cursor_command_options_new(void);

Description
-----------

Creates a new :symbol:`mongoac_run_cursor_command_options_t` in its default-initialized state.

Must be destroyed with :symbol:`mongoac_run_cursor_command_options_destroy()`.

.. seealso::

   | :symbol:`mongoac_run_cursor_command_options_clone`
   | :symbol:`mongoac_run_cursor_command_options_destroy`
