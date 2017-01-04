:man_page: bson_string_t

bson_string_t
=============

String Building Abstraction

Synopsis
--------

.. code-block:: c

  #include <bson.h>

  typedef struct {
     char *str;
     uint32_t len;
     uint32_t alloc;
  } bson_string_t;

Description
-----------

:symbol:`bson_string_t <bson_string_t>` is an abstraction for building strings. As chunks are added to the string, allocations are performed in powers of two.

This API is useful if you need to build UTF-8 encoded strings.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_snprintf
    bson_strdup
    bson_strdup_printf
    bson_strdupv_printf
    bson_strfreev
    bson_string_append
    bson_string_append_c
    bson_string_append_printf
    bson_string_append_unichar
    bson_string_free
    bson_string_new
    bson_string_truncate
    bson_strncpy
    bson_strndup
    bson_strnlen
    bson_utf8_escape_for_json
    bson_utf8_from_unichar
    bson_utf8_get_char
    bson_utf8_next_char
    bson_utf8_validate
    bson_vsnprintf

Example
-------

.. code-block:: c

  bson_string_t *str;

  str = bson_string_new (NULL);
  bson_string_append_printf (str, "%d %s %f\n", 0, "some string", 0.123);
  printf ("%s\n", str->str);

  bson_string_free (str, true);

.. tip:

  You can call :symbol:`bson_string_free() <bson_string_free>` with ``false`` if you would like to take ownership of ``str->str``. Some APIs that do this might call ``return bson_string_free (str, false);`` after building the string.

