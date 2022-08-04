:man_page: bson_iter_visit_all_v2

bson_iter_visit_all_v2()
========================

Synopsis
--------

.. code-block:: c

  enum bson_iter_visit_flags {
    BSON_ITER_VISIT_NOFLAGS = 0,
    BSON_ITER_VISIT_VALIDATE_KEYS = 1 << 0,
    BSON_ITER_VISIT_VALIDATE_UTF8 = 1 << 1,
    BSON_ITER_VISIT_VALIDATE_REGEX = 1 << 2,
    BSON_ITER_VISIT_VALIDATE_CODE = 1 << 3,
    BSON_ITER_VISIT_VALIDATE_DBPOINTER = 1 << 4,
    BSON_ITER_VISIT_VALIDATE_SYMBOL = 1 << 5,
    BSON_ITER_VISIT_ALLOW_NUL_IN_UTF8 = 1 << 6,
    BSON_ITER_VISIT_VALIDATE_STRINGS =
        BSON_ITER_VISIT_VALIDATE_KEYS | BSON_ITER_VISIT_VALIDATE_UTF8 |
        BSON_ITER_VISIT_VALIDATE_REGEX | BSON_ITER_VISIT_VALIDATE_CODE |
        BSON_ITER_VISIT_VALIDATE_DBPOINTER | BSON_ITER_VISIT_VALIDATE_SYMBOL,
    BSON_ITER_VISIT_VALIDATE_VALUES =
        BSON_ITER_VISIT_VALIDATE_UTF8 | BSON_ITER_VISIT_VALIDATE_REGEX |
        BSON_ITER_VISIT_VALIDATE_CODE | BSON_ITER_VISIT_VALIDATE_DBPOINTER |
        BSON_ITER_VISIT_VALIDATE_SYMBOL,
    BSON_ITER_VISIT_DEFAULT = BSON_ITER_VISIT_VALIDATE_KEYS |
                              BSON_ITER_VISIT_VALIDATE_VALUES |
                              BSON_ITER_VISIT_ALLOW_NUL_IN_UTF8,
  };

  bool
  bson_iter_visit_all_v2 (bson_iter_t *iter,
                          const bson_visitor_t *visitor,
                          const bson_iter_visit_flags flags.
                          void *data);

Parameters
----------

* ``iter``: A :symbol:`bson_iter_t`.
* ``visitor``: A :symbol:`bson_visitor_t`.
* ``flags``: A set of bit flags from ``bson_iter_visit_flags`` to control
  visitation (See below).
* ``data``: Optional data for ``visitor``.

Description
-----------

A convenience function to iterate all remaining fields of ``iter`` using the
callback vtable provided by ``visitor``.

This function is an extension of :symbol:`bson_iter_visit_all`, with the
additional ``flags`` parameter that can be used to control the behavior of the
visit iteration. The following flags are defined:

``BSON_ITER_VISIT_NOFLAGS``

  None of the behaviors requested by any other options are used.

``BSON_ITER_VISIT_VALIDATE_KEYS``

  Validate that element keys are valid UTF-8-encoded strings.

``BSON_ITER_VISIT_VALIDATE_UTF8``

  Validate that UTF-8 text elements are valid UTF-8-encoded strings.

``BSON_ITER_VISIT_VALIDATE_REGEX``

  Validate regular expression elements to contain valid UTF-8 encoded strings.

``BSON_ITER_VISIT_VALIDATE_CODE``

  Validate code elements that the code component is a valid UTF-8-encoded
  string.

``BSON_ITER_VISIT_VALIDATE_DBPOINTER``

  Validate that DBPointer element names are valid UTF-8-encoded strings.

``BSON_ITER_VISIT_VALIDATE_SYMBOL``

  Validate that symbol elements are valid UTF-8-encoded strings.

``BSON_ITER_VISIT_ALLOW_NUL_IN_UTF8``

  When validating any UTF-8 strings, permit a zero ``0x00`` code unit as a valid
  UTF-8 code unit.

``BSON_ITER_VISIT_VALIDATE_STRINGS``

  Validate all strings in all components in top-level elements of the document.

``BSON_ITER_VISIT_VALIDATE_VALUES``

  Validate all values in all top-level elements of the document.

``BSON_ITER_VISIT_DEFAULT``

  Validate all keys and values. Permits a zero ``0x00`` UTF-8 code unit in
  strings.

  This flag has the same behavior as :symbol:`bson_iter_visit_all`.

.. note::

  If a requested element fails validation, the ``bson_iter_visit_all_v2`` call
  will return and indicate the position of the erring element via ``iter``.

Returns
-------

Returns true if visitation was prematurely stopped by a callback function. Returns false either because all elements were visited *or* due to corrupt BSON.

See :symbol:`bson_visitor_t` for examples of how to set your own callbacks to provide information about the location of corrupt or unsupported BSON document entries.

