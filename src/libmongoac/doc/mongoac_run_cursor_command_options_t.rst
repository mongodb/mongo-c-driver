:man_page: mongoac_run_cursor_command_options_t

mongoac_run_cursor_command_options_t
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/run_cursor_command_options-fwd.h>

   typedef struct mongoac_run_cursor_command_options_t mongoac_run_cursor_command_options_t;

   #include <mongoac/run_cursor_command_options.h>

Description
-----------

Represents `mongodb::options::RunCursorCommandOptions <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html>`_.

Fields
------

The following fields are supported:

- `batch size <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html#structfield.batch_size>`_
- `comment <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html#structfield.comment>`_
- `cursor type <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html#structfield.cursor_type>`_
- `max time <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html#structfield.max_time>`_
- :symbol:`read preference <mongoac_read_preference_t>` (via `SelectionCriteria <https://docs.rs/mongodb/latest/mongodb/options/struct.RunCursorCommandOptions.html#structfield.selection_criteria>`_)

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_run_cursor_command_options_destroy
    mongoac_run_cursor_command_options_clone
    mongoac_run_cursor_command_options_new

    mongoac_run_cursor_command_options_get_batch_size
    mongoac_run_cursor_command_options_get_batch_size_is_none
    mongoac_run_cursor_command_options_get_comment
    mongoac_run_cursor_command_options_get_comment_is_none
    mongoac_run_cursor_command_options_get_comment_string
    mongoac_run_cursor_command_options_get_cursor_type
    mongoac_run_cursor_command_options_get_cursor_type_is_none
    mongoac_run_cursor_command_options_get_max_time_as_secs
    mongoac_run_cursor_command_options_get_max_time_is_none
    mongoac_run_cursor_command_options_get_max_time_subsec_millis
    mongoac_run_cursor_command_options_get_read_preference

    mongoac_run_cursor_command_options_set_batch_size
    mongoac_run_cursor_command_options_set_batch_size_none
    mongoac_run_cursor_command_options_set_comment
    mongoac_run_cursor_command_options_set_comment_none
    mongoac_run_cursor_command_options_set_comment_string
    mongoac_run_cursor_command_options_set_cursor_type
    mongoac_run_cursor_command_options_set_cursor_type_none
    mongoac_run_cursor_command_options_set_max_time_from_millis
    mongoac_run_cursor_command_options_set_max_time_from_secs
    mongoac_run_cursor_command_options_set_max_time_none
    mongoac_run_cursor_command_options_set_read_preference
