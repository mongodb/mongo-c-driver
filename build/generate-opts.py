#!/usr/bin/env python
#
# Copyright 2017-present MongoDB, Inc.
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

"""IDL for functions that take flexible options as a bson_t.

Defines the options accepted by functions that receive a const bson_t *opts,
for example mongoc_collection_find_with_opts, mongoc_collection_insert_one,
and many others.

Generates struct types, options parsing code, and RST documentation.

Written for Python 2.6+, requires Jinja 2 for templating.
"""

from collections import OrderedDict
from os.path import basename, dirname, join as joinpath, normpath

from jinja2 import Environment, FileSystemLoader  # Please "pip install jinja2".

this_dir = dirname(__file__)
template_dir = joinpath(this_dir, 'opts_templates')
src_dir = normpath(joinpath(this_dir, '../src/mongoc'))


class Struct(OrderedDict):
    def __init__(self, items, is_base=False):
        """Define a struct. If is_base=True, don't generate a parse function."""
        super(Struct, self).__init__(items)
        self.is_base = is_base


opts_structs = OrderedDict([
    ('mongoc_find_one_opts_t', Struct([
        ('projection', {'type': 'document'}),
        ('sort', {'type': 'document'}),
        ('skip', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('limit', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('batchSize', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('exhaust', {'type': 'bool'}),
        ('hint', {'type': 'bson_value_t'}),
        ('allowPartialResults', {'type': 'bool'}),
        ('awaitData', {'type': 'bool'}),
        ('collation', {'type': 'document'}),
        ('comment', {'type': 'utf8'}),
        ('max', {'type': 'document'}),
        ('maxScan', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('maxTimeMS', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('maxAwaitTimeMS', {
            'type': 'int64_t',
            'convert': '_mongoc_convert_int64_positive'
        }),
        ('min', {'type': 'document'}),
        ('noCursorTimeout', {'type': 'bool'}),
        ('oplogReplay', {'type': 'bool'}),
        ('returnKey', {'type': 'bool'}),
        ('showRecordId', {'type': 'bool'}),
        ('singleBatch', {'type': 'bool'}),
        ('snapshot', {'type': 'bool'}),
        ('tailable', {'type': 'bool'})
    ])),
    ('mongoc_crud_opts_t', Struct([
        ('writeConcern', {
            'type': 'document',
            'convert': '_mongoc_convert_write_concern',
            'help': 'Construct a :symbol:`mongoc_write_concern_t` and use :symbol:`mongoc_write_concern_append` to add the write concern to ``opts``. See the example code for :symbol:`mongoc_client_write_command_with_opts`.'
        }),
        ('sessionId', {
            'type': 'mongoc_client_session_t *',
            'convert': '_mongoc_convert_session_id',
            'field': 'client_session',
            'help': 'Construct a :symbol:`mongoc_client_session_t` with :symbol:`mongoc_client_start_session` and use :symbol:`mongoc_client_session_append` to add the session to ``opts``. See the example code for :symbol:`mongoc_client_session_t`.'
        }),
        ('validate', {
            'type': 'bson_validate_flags_t',
            'convert': '_mongoc_convert_validate_flags',
            'help': 'Construct a bitwise-or of all desired `bson_validate_flags_t <http://mongoc.org/libbson/current/bson_validate_with_error.html>`_. Set to ``0`` to skip client-side validation of the provided BSON documents.'
        }),
        ('bypassDocumentValidation', {
            'type': 'bool',
            'help': 'Set to ``true`` to skip server-side schema validation of the provided BSON documents.'
        }),
        ('bdv_set', {
            'type': 'bool',
            'help': 'Set to ``true`` to distinguish when the user explicitly sets bypassDocumentValidation.'
        }),
        ('collation', {
            'type': 'document',
            'help': 'Configure textual comparisons. See :ref:`Setting Collation Order <setting_collation_order>`, and `the MongoDB Manual entry on Collation <https://docs.mongodb.com/manual/reference/collation/>`_.'
        }),
    ], is_base=True)),
    ('mongoc_insert_one_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'})
    ])),
    ('mongoc_insert_many_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'}),
        ('ordered', {'type': 'bool'})
    ])),
    ('mongoc_delete_one_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'})
    ])),
    ('mongoc_delete_many_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'}),
        ('ordered', {'type': 'bool'})
    ])),
    ('mongoc_update_one_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'})
    ])),
    ('mongoc_update_many_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'}),
        ('ordered', {'type': 'bool'})
    ])),
    ('mongoc_delete_one_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'})
    ])),
    ('mongoc_replace_one_opts_t', Struct([
        ('crud', {'type': 'mongoc_crud_opts_t'}),
    ])),
])

header_comment = """/**************************************************
 *
 * Generated by build/%s.
 *
 * DO NOT EDIT THIS FILE.
 *
 *************************************************/
/* clang-format off */""" % basename(__file__)


def paths(struct):
    """Sequence of path, option name, option info."""
    for option_name, info in struct.items():
        the_type = info['type']
        the_field = info.get('field', option_name)
        if the_type in opts_structs:
            # E.g., the type is mongoc_crud_opts_t. Recurse.
            for path, sub_option_name, sub_info in paths(opts_structs[the_type]):
                yield ('%s.%s' % (the_field, path),
                       sub_option_name,
                       sub_info)
        else:
            yield the_field, option_name, info


env = Environment(loader=FileSystemLoader(template_dir),
                  trim_blocks=True,
                  extensions=['jinja2.ext.loopcontrols'])

files = ["mongoc-opts-private.h", "mongoc-opts.c"]

for file_name in files:
    print(file_name)
    with open(joinpath(src_dir, file_name), 'w+') as f:
        t = env.get_template(file_name + ".template")
        f.write(t.render(globals()))
        f.write('\n')

# TODO: also generate RST text files in doc/includes
