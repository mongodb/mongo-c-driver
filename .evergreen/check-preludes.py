#!/usr/bin/env python
#
# Copyright 2019-present MongoDB, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Check that public libbson/libmongoc headers all include the prelude line.
"""
import os
import re
import sys
import glob

if len(sys.argv) != 2:
    print("Usage: python check-preludes.py <mongo-c-driver directory>")
    exit(1)

mongoc_prefix = "src/libmongoc/src/mongoc/"
bson_prefix = "src/libbson/src/bson/"
common_prefix = "src/common/"

checks = [
    {
        "name": "libmongoc",
        "headers": glob.glob(mongoc_prefix + "*.h"),
        "exclusions": [
            mongoc_prefix + "mongoc-prelude.h",
            mongoc_prefix + "mongoc.h"
        ],
        "include": "#include \"mongoc-prelude.h\""
    },
    {
        "name": "libbson",
        "headers": glob.glob( bson_prefix + "*.h"),
        "exclusions": [
            bson_prefix + "bson-prelude.h",
            bson_prefix + "bson.h"
        ],
        "include": "#include \"bson-prelude.h\""
    },
    {
        "name": "common",
        "headers": glob.glob( common_prefix + "*.h"),
        "exclusions": [ common_prefix + "common-prelude.h" ],
        "include": "#include \"common-prelude.h\""
    },
]

failed = False
for check in checks:
    print("Checking headers for %s" % check["name"])
    assert (len(check["headers"]) > 0)
    for header in check["headers"]:
        if header in check["exclusions"]:
            continue
        with open(header, "r") as file:
            if not check["include"] in file.read().splitlines():
                print("%s did not include prelude" % header)
                failed = True
if failed:
    sys.exit(1)
 
print("All checks passed")