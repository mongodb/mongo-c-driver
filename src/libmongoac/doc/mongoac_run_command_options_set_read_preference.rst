:man_page: mongoac_run_command_options_set_read_preference

mongoac_run_command_options_set_read_preference()
=================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_command_options.h>

   void mongoac_run_command_options_set_read_preference(mongoac_run_command_options_t *options,
                                                        const mongoac_read_preference_t *v);

Description
-----------

Sets the :symbol:`"read preference" <mongoac_read_preference_t>` field for the given :symbol:`mongoac_run_command_options_t`.

When ``options`` is null, does nothing.

When ``v`` is null, the field is set to ``None``.

.. seealso::

   | :symbol:`mongoac_run_command_options_get_read_preference`
