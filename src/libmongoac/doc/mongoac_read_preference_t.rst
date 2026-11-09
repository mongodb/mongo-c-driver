:man_page: mongoac_read_preference_t

mongoac_read_preference_t
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference-fwd.h>

   typedef struct mongoac_read_preference_t mongoac_read_preference_t;

   #include <mongoac/read_preference.h>

   typedef int32_t mongoac_read_preference_mode_t;

   // Read Modes
   #define MONGOAC_READ_PREFERENCE_MODE_PRIMARY 0
   #define MONGOAC_READ_PREFERENCE_MODE_SECONDARY 1
   #define MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED 2
   #define MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED 3
   #define MONGOAC_READ_PREFERENCE_MODE_NEAREST 4
   #define MONGOAC_READ_PREFERENCE_MODE_UNKNOWN /* (see below) */

Description
-----------

Represents a `mongodb::options::ReadPreference <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html>`_.

.. _mongoac_read_preference_mode_t:

Modes
-----

The following `modes <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html>`_ are supported:

- ``MONGOAC_READ_PREFERENCE_MODE_PRIMARY`` ("primary"): `ReadPreference::Primary <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html#variant.Primary>`_.
- ``MONGOAC_READ_PREFERENCE_MODE_SECONDARY`` ("secondary"): `ReadPreference::Secondary <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html#variant.Secondary>`_.
- ``MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED`` ("primary preferred"): `ReadPreference::PrimaryPreferred <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html#variant.PrimaryPreferred>`_.
- ``MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED`` ("secondary preferred"): `ReadPreference::SecondaryPreferred <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html#variant.SecondaryPreferred>`_.
- ``MONGOAC_READ_PREFERENCE_MODE_NEAREST`` ("nearest"): `ReadPreference::Nearest <https://docs.rs/mongodb/latest/mongodb/options/enum.ReadPreference.html#variant.Nearest>`_.
- ``MONGOAC_READ_PREFERENCE_MODE_UNKNOWN`` ("unknown"): Unknown value. Defaults to ``INT32_MIN`` unless otherwise specified.

Mode "primary" is the default state.

Fields
-------

The following `ReadPreferenceOptions <https://docs.rs/mongodb/latest/mongodb/options/struct.ReadPreferenceOptions.html>`_ fields are supported for all modes except "primary":

- `max_staleness <https://docs.rs/mongodb/latest/mongodb/options/struct.ReadPreferenceOptions.html#structfield.max_staleness>`_

Fields are preserved across all modes except "primary".

When the mode is set to "primary", all fields are reset to ``None``.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_read_preference_destroy
    mongoac_read_preference_clone
    mongoac_read_preference_new

    mongoac_read_preference_get_max_staleness_as_secs
    mongoac_read_preference_get_max_staleness_is_none
    mongoac_read_preference_get_mode

    mongoac_read_preference_set_max_staleness_from_secs
    mongoac_read_preference_set_max_staleness_none
    mongoac_read_preference_set_mode
