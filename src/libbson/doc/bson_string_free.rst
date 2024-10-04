:man_page: bson_string_free

bson_string_free()
==================

.. warning::
   .. deprecated:: 1.29.0

      This function is deprecated and should not be used in new code.

Synopsis
--------

.. code-block:: c

  char *
  bson_string_free (bson_string_t *string, bool free_segment);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``free_segment``: A bool indicating whether ``string->str`` should be freed.

Description
-----------

Frees the ``bson_string_t`` structure and optionally ``string->str``.

Returns
-------

``string->str`` if ``free_segment`` is false, otherwise ``NULL``.

