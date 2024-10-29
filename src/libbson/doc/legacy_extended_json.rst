:man_page: libbson_legacy_extended_json

Libbson Legacy Extended JSON
============================

libbson can produce a non-portable Legacy Extended JSON format.

.. warning::
   
   Use of the Legacy Extended JSON format is discouraged. Prefer Canonical Extended JSON or Relaxed Extended JSON for portability.

`MongoDB Extended JSON (v2)`_ describes the preferred Relaxed Extended JSON format and Canonical Extended Formats

libbson's Legacy Extended JSON format matches Relaxed Extended JSON with the following exceptions. Notation is borrowed from `MongoDB Extended JSON (v2)`_:

.. list-table::
   :header-rows: 1

   * - Type
     - Legacy Extended JSON

   * - Binary
     - .. code:: json
         
          { "$binary": "<payload>", "$type": "<t>" }

   * - Date
     - .. code:: json

          { "$date" : "<millis>" }

   * - Regular Expression
     - .. code:: json

          { "$regex" : "<regexPattern>", "$options" : "<options>" }

   * - DBPointer (deprecated)
     - .. code:: json

          { "$ref" : "<collection namespace>", "$id" : "<ObjectId bytes>" }

   * - Symbol (deprecated)
     - .. code:: json

          "<string>"

   * - Double infinity
     - ``infinity`` or ``inf`` without quotes. Implementation defined. Produces invalid JSON. 

   * - Double NaN
     - ``nan`` or ``nan(n-char-sequence)``. Implementation defined. Produces invalid JSON.

.. _BSON: https://bsonspec.org/
.. _MongoDB Extended JSON (v2): https://www.mongodb.com/docs/manual/reference/mongodb-extended-json/

.. only:: html

  .. include:: includes/seealso/bson-as-json.txt
