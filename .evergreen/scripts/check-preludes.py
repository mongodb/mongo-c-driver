#!/usr/bin/env python3
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
import sys
import glob

if len(sys.argv) != 2:
    print("Usage: python check-preludes.py <mongo-c-driver directory>")
    sys.exit(1)

MONGOC_PREFIX = "src/libmongoc/src/mongoc/"
BSON_PREFIX = "src/libbson/src/bson/"
COMMON_PREFIX = "src/common/"

checks = [
    {
        "name": "libmongoc",
        "headers": glob.glob(MONGOC_PREFIX + "*.h"),
        "exclusions": [
            MONGOC_PREFIX + "mongoc-prelude.h",
            MONGOC_PREFIX + "mongoc.h"
        ],
        "include": "#include \"mongoc-prelude.h\""
    },
    {
        "name": "libbson",
        "headers": glob.glob(BSON_PREFIX + "*.h"),
        "exclusions": [
            BSON_PREFIX + "bson-prelude.h",
            BSON_PREFIX + "bson-dsl.h",
            BSON_PREFIX + "bson.h"
        ],
        "include": "#include \"bson-prelude.h\""
    },
    {
        "name": "common",
        "headers": glob.glob(COMMON_PREFIX + "*.h"),
        "exclusions": [COMMON_PREFIX + "common-prelude.h"],
        "include": "#include \"common-prelude.h\""
    },
]

for check in checks:
    NAME = check["name"]
    print(f"Checking headers for {NAME}")
    assert len(check["headers"]) > 0
    for header in check["headers"]:
        if header in check["exclusions"]:
            continue
        with open(header, mode="r", encoding="utf-8") as file:
            if not check["include"] in file.read().splitlines():
                print(f"{header} did not include prelude")
                sys.exit(1)

print("All checks passed")
