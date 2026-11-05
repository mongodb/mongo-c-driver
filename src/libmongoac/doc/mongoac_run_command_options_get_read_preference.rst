:man_page: mongoac_run_command_options_get_read_preference

mongoac_run_command_options_get_read_preference()
=================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_command_options.h>

   const mongoac_read_preference_t *mongoac_run_command_options_get_read_preference(const mongoac_run_command_options_t *options);

Description
-----------

Gets the :symbol:`"read preference" <mongoac_read_preference_t>` field for the given :symbol:`mongoac_run_command_options_t`.

When ``options`` is null, returns a null pointer.

When the field is ``None``, returns a null pointer.

.. seealso::

   | :symbol:`mongoac_run_command_options_set_read_preference`
