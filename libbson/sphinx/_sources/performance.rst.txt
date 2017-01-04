:man_page: bson_performance

Performance Notes
=================

.. _performance_array_element_key_building:

Array Element Key Building
--------------------------

When writing marshaling layers between higher level languages and Libbson, you will eventually need to build keys for array elements. Each element in a BSON array has a monotonic string key like ``"0"``, ``"1"``, etc. Using ``snprintf()`` and others tend to be rather slow on most ``libc`` implementations. Therefore, Libbson provides :symbol:`bson_uint32_to_string() <bson_uint32_to_string>` to improve this. Using this function allows an internal fast path to be used for numbers less than 1000 which is the vast majority of arrays. If the key is larger than that, a fallback of ``snprintf()`` will be used.

.. code-block:: c

  char str[16];
  const char *key;
  uint32_t i;

  for (i = 0; i < 10; i++) {
     bson_uint32_to_string (i, &key, str, sizeof str);
     printf ("Key: %s\n", key);
  }

For more information, see :symbol:`bson_uint32_to_string() <bson_uint32_to_string>`.

