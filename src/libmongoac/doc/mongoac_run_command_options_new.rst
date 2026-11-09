:man_page: mongoac_run_command_options_new

mongoac_run_command_options_new()
=================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_command_options.h>

   mongoac_run_command_options_t *mongoac_run_command_options_new(void);

Description
-----------

Returns a new :symbol:`mongoac_run_command_options_t` in its default state.

Must be destroyed with :symbol:`mongoac_run_command_options_destroy()`.

.. seealso::

   | :symbol:`mongoac_run_command_options_clone`
   | :symbol:`mongoac_run_command_options_destroy`
