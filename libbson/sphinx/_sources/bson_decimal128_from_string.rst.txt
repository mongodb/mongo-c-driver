:man_page: bson_decimal128_from_string

bson_decimal128_from_string()
=============================

Synopsis
--------

.. code-block:: c

  bool
  bson_decimal128_from_string (const char *string, bson_decimal128_t *dec);

Parameters
----------

* ``string``: A string containing ASCII encoded Decimal128.
* ``dec``: A :symbol:`bson_decimal128_t <bson_decimal128_t>`.

Description
-----------

Parses the string containing ascii encoded decimal128 and initialize the bytes in ``decimal128``.

Example
-------

.. code-block:: c

  bson_decimal128_t dec;
  bson_decimal128_from_string ("1.00", &dec);

