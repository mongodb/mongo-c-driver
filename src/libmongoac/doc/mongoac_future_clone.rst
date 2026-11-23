:man_page: mongoac_future_clone

mongoac_future_clone()
======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   mongoac_future_t *mongoac_future_clone(const mongoac_future_t *future);

Description
-----------

Create a new :symbol:`mongoac_future_t` with the same shared state as ``future``.

When ``future`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_future_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_future_destroy`
   | :symbol:`mongoac_future_t`
