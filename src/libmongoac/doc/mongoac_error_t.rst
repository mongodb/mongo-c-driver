:man_page: mongoac_error_t

mongoac_error_t
===============

Synopsis
--------

.. code-block:: c

   #include <mongoac/error-fwd.h>

   typedef struct mongoac_error_t mongoac_error_t;

   #include <mongoac/error.h>

   typedef int32_t mongoac_error_category_t;
   typedef int32_t mongoac_error_code_t;

   // Error Categories
   #define MONGOAC_ERROR_CATEGORY_NONE 0
   #define MONGOAC_ERROR_CATEGORY_MONGOAC 1
   #define MONGOAC_ERROR_CATEGORY_UNKNOWN /* (see below) */

   // Error Codes
   #define MONGOAC_ERROR_CODE_OK 0
   #define MONGOAC_ERROR_CODE_INVALID_ARGUMENT 1
   #define MONGOAC_ERROR_CODE_RUNTIME_ERROR 2
   #define MONGOAC_ERROR_CODE_UNKNOWN /* (see below) */

Description
-----------

Represents various error codes returned by the mongoac library.

The error category maps the error code value to a specific origin.
The error code describes an error specific to the associated error category.

.. note::

   This pattern is based on ``std::error_code`` from the C++ standard library.

.. important::

   As a library-wide convention, any mongoac function which accepts an optional non-const pointer to ``mongoac_error_t``
   as its last parameter will reset the error object to its default state before any further operations.

Error Categories
----------------

The following error categories are defined for errors returned by the mongoac library.

- ``MONGOAC_ERROR_CATEGORY_NONE``: None (default state).
- ``MONGOAC_ERROR_CATEGORY_MONGOAC``: The mongoac library.
- ``MONGOAC_ERROR_CATEGORY_UNKNOWN``: All other (unnamed) error categories. Defaults to ``INT32_MIN`` unless otherwise specified.

Error Codes
-----------

The following error codes are defined for the ``MONGOAC_ERROR_CATEGORY_MONGOAC`` category:

- ``MONGOAC_ERROR_CODE_OK``: None (default state).
- ``MONGOAC_ERROR_CODE_INVALID_ARGUMENT``: One or more arguments to the associated function were invalid.
- ``MONGOAC_ERROR_CODE_RUNTIME_ERROR``: A runtime error occurred.
- ``MONGOAC_ERROR_CODE_UNKNOWN``: All other (unnamed) error codes. Defaults to ``INT32_MIN`` unless otherwise specified.

Error Messages
--------------

An optional error message may provide additional information describing the corresponding error.
When no error message is available (e.g. in the default state), :symbol:`mongoac_error_message()` returns a null string.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_error_destroy
    mongoac_error_clone
    mongoac_error_new

    mongoac_error_category
    mongoac_error_code
    mongoac_error_message

    mongoac_error_clear
    mongoac_error_set
