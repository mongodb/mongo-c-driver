####################
Queryable Encryption
####################

Using Queryable Encryption requires MongoDB Server 7.0 or higher.

See the MongoDB Manual for `Queryable Encryption
<https://www.mongodb.com/docs/manual/core/queryable-encryption/>`_ for more
information about the feature.

API related to the "rangePreview" algorithm is still experimental and subject to breaking changes!

Queryable Encryption in older MongoDB Server versions
-----------------------------------------------------

MongoDB Server 6.0 introduced Queryable Encryption as a Public Technical
Preview. MongoDB Server 7.0 includes backwards breaking changes to the Queryable
Encryption protocol.

The backwards breaking changes are applied in the client protocol in
libmongocrypt 1.8.0. libmongoc 1.24.0 requires libmongocrypt 1.8.0 or newer.
libmongoc 1.24.0 no longer supports Queryable Encryption in MongoDB Server <7.0.
Using Queryable Encryption libmongoc 1.24.0 and higher requires MongoDB Server
>=7.0.

Using Queryable Encryption with libmongocrypt<1.8.0 on a MongoDB Server>=7.0, or
using libmongocrypt>=1.8.0 on a MongoDB Server<6.0 will result in a server error
when using the incompatible protocol.

.. seealso::

    | The MongoDB Manual for `Queryable Encryption <https://www.mongodb.com/docs/manual/core/queryable-encryption/>`_
    