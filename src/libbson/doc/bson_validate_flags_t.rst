:man_page: bson_validate_flags_t

bson_validate_flags_t
=====================

Document validation options

Synopsis
--------

.. code-block:: c

  #include <bson/bson-types.h>

  typedef enum {
    BSON_VALIDATE_NONE = 0,
    BSON_VALIDATE_UTF8 = (1 << 0),
    BSON_VALIDATE_DOLLAR_KEYS = (1 << 1),
    BSON_VALIDATE_DOT_KEYS = (1 << 2),
    BSON_VALIDATE_UTF8_ALLOW_NULL = (1 << 3),
    BSON_VALIDATE_EMPTY_KEYS = (1 << 4),
    BSON_VALIDATE_CORRUPT = (1 << 5),
  } bson_validate_flags_t;

Description
-----------

``bson_validate_flags_t`` is a set of binary flags which may be combined to specify a level of BSON document validation.

A value of ``0``, ``false``, or ``BSON_VALIDATE_NONE`` equivalently requests the minimum applicable level of validation.

In the context of validation APIs :symbol:`bson_validate()`, :symbol:`bson_validate_with_error()`, and :symbol:`bson_validate_with_error_and_offset()` the minimum validation still guarantees that a document can be successfully traversed by :symbol:`bson_iter_visit_all()`.

Higher level APIs using this type may have different minimum validation levels. For example, ``libmongoc`` functions that take ``bson_validate_flags_t`` use ``0`` to mean the document contents are not visited and malformed headers will not be detected by the client.

Each defined flag aside from ``BSON_VALIDATE_NONE`` describes an optional validation feature that may be enabled, alone or in combination with other features:

* ``BSON_VALIDATE_NONE`` Minimum level of validation; in ``libbson``, validates element headers.
* ``BSON_VALIDATE_UTF8`` All keys and string values are checked for invalid UTF-8.
* ``BSON_VALIDATE_UTF8_ALLOW_NULL`` String values are allowed to have embedded NULL bytes.
* ``BSON_VALIDATE_DOLLAR_KEYS`` Prohibit keys that start with ``$`` outside of a "DBRef" subdocument.
* ``BSON_VALIDATE_DOT_KEYS`` Prohibit keys that contain ``.`` anywhere in the string.
* ``BSON_VALIDATE_EMPTY_KEYS`` Prohibit zero-length keys.
* ``BSON_VALIDATE_CORRUPT`` is not a control flag, but is used as an error code
  when a validation routine encounters corrupt BSON data.

.. seealso::

  | :symbol:`bson_validate()`, :symbol:`bson_validate_with_error()`, :symbol:`bson_validate_with_error_and_offset()`.

  | :symbol:`bson_visitor_t` can be used for custom validation, :ref:`example_custom_validation`.
