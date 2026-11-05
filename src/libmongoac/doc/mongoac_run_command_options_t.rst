:man_page: mongoac_run_command_options_t

mongoac_run_command_options_t
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_command_options-fwd.h>

   typedef struct mongoac_run_command_options_t mongoac_run_command_options_t;

   #include <mongoac/run_command_options.h>

Description
-----------

Represents `mongodb::options::RunCommandOptions <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCommandOptions.html>`_.

Used with :symbol:`mongoac_database_run_command()`.

Fields
------

The following fields are supported:

- :symbol:`read preference <mongoac_read_preference_t>` (via `SelectionCriteria <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCommandOptions.html#structfield.selection_criteria>`_).

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_run_command_options_destroy
    mongoac_run_command_options_clone
    mongoac_run_command_options_new

    mongoac_run_command_options_get_read_preference

    mongoac_run_command_options_set_read_preference
