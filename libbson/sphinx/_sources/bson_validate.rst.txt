:man_page: bson_validate

bson_validate()
===============

Synopsis
--------

.. code-block:: c

  typedef enum {
     BSON_VALIDATE_NONE = 0,
     BSON_VALIDATE_UTF8 = (1 << 0),
     BSON_VALIDATE_DOLLAR_KEYS = (1 << 1),
     BSON_VALIDATE_DOT_KEYS = (1 << 2),
     BSON_VALIDATE_UTF8_ALLOW_NULL = (1 << 3),
     BSON_VALIDATE_EMPTY_KEYS = (1 << 4),
  } bson_validate_flags_t;

  bool
  bson_validate (const bson_t *bson, bson_validate_flags_t flags, size_t *offset);

Parameters
----------

* ``bson``: A :symbol:`bson_t <bson_t>`.
* ``flags``: A bitwise-or of all desired :symbol:`bson_validate_flags_t <bson_validate>`.
* ``offset``: A location for the offset within ``bson`` where the error ocurred.

Description
-----------

Validates a BSON document by walking through the document and inspecting the fields for valid content.

You can modify how the validation occurs through the use of the ``flags`` parameter. A description of their effect is below.

* ``BSON_VALIDATE_UTF8`` will request that all UTF-8 strings are checked to contain valid UTF-8 sequences. This is expensive and disabled by default.
* ``BSON_VALIDATE_UTF8_ALLOW_NULL`` will specify that UTF-8 strings are allowed to have embedded NULL bytes. Many UTF-8 implementations use a 2-byte squence for embedded NULLs so that they work with stanard libc functions. Libbson expects this by default.
* ``BSON_VALIDATE_DOLLAR_KEYS`` will request that all key names are checked to ensure they do not start with the ASCII dollar character (``$``).
* ``BSON_VALIDATE_DOT_KEYS`` will request that all key names are checked to ensure they do not contain an ASCII dot (``.``) character.
* ``BSON_VALIDATE_EMPTY_KEYS`` will request that zero-length key names are prohibited.

Returns
-------

Returns true if ``bson`` is valid; otherwise false and ``offset`` is set to the byte offset where the error was detected.

