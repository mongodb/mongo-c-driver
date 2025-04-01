:man_page: bson_copy_to_excluding_noinit_va

bson_copy_to_excluding_noinit_va()
==================================

Synopsis
--------

.. code-block:: c

   void
   bson_copy_to_excluding_noinit_va (const bson_t *src,
                                     bson_t *dst,
                                     const char *first_exclude,
                                     va_list args);

Parameters
----------

* ``src``: A :symbol:`bson_t`.
* ``dst``: A :symbol:`bson_t`.
* ``first_exclude``: The first field name to exclude.
* ``args``: A va_list.

Description
-----------

The :symbol:`bson_copy_to_excluding_noinit_va()` function shall copy all fields from ``src`` to ``dst`` except those specified by ``first_exclude`` and ``args``.

Does **not** call :symbol:`bson_init` on ``dst``.

.. seealso::

  | :symbol:`bson_copy_to_excluding_noinit`

