:man_page: mongoc_structured_log_get_named_component

mongoc_structured_log_get_named_component()
===========================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_structured_log_get_named_component (const char *name, mongoc_structured_log_component_t *out);

Look up a component by name. Case insensitive.

Parameters
----------

* ``name``: A name to look up as a log component.
* ``out``: On success, the corresponding :symbol:`mongoc_structured_log_component_t` is written here.

Returns
-------

If the component name is known, returns ``true`` and writes the component enum to ``*out``.
If the component name is not known, returns ``false`` and does not write ``*out``.

.. seealso::

  | :doc:`structured_log`
