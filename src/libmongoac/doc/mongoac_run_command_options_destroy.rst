:man_page: mongoac_run_command_options_destroy

mongoac_run_command_options_destroy()
=====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_command_options.h>

   void mongoac_run_command_options_destroy(mongoac_run_command_options_t *options);

Description
-----------

Destroys the given :symbol:`mongoac_run_command_options_t`.

When ``options`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_run_command_options_clone`
   | :symbol:`mongoac_run_command_options_new`
