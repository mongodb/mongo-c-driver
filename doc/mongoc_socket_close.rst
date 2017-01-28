:man_page: mongoc_socket_close

mongoc_socket_close()
=====================

Synopsis
--------

.. code-block:: c

  int
  mongoc_socket_close (mongoc_socket_t *socket);

Parameters
----------

* ``socket``: A :symbol:`mongoc_socket_t`.

Description
-----------

This function is a wrapper around the BSD socket ``shutdown()`` interface. It provides better portability between UNIX-like and Microsoft Windows platforms.

Returns
-------

0 on success, -1 on failure to close the socket.

