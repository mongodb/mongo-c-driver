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
import re

from jinja2 import Environment, FileSystemLoader  # Please "pip install jinja2".

this_dir = dirname(__file__)
template_dir = joinpath(this_dir, 'opts_templates')
src_dir = normpath(joinpath(this_dir, '../src/mongoc'))
doc_includes = normpath(joinpath(this_dir, '../doc/includes'))


class Struct(OrderedDict):
    def __init__(self, instance_type, items, generate_rst=True, **defaults):
        """Define an options struct.

        - instance_type: A type like mongoc_client_t, mongoc_database_t, etc.
        - items: List of pairs: (optionName, info)
        - defaults: Initial values for options
        """
        super(Struct, self).__init__(items)
        if instance_type is not None:
            self.instance_type = instance_type
            self.instance_name = re.sub(r'mongoc_(\w+)_t', r'\1', instance_type)
        else:
            self.instance_type = self.instance_name = ''
        self.is_shared = False
        self.generate_rst = generate_rst
        self.defaults = defaults

    def default(self, item, fallback):
        return self.defaults.get(item, fallback)


class Shared(Struct):
    def __init__(self, items, **defaults):
        """Define a struct that is shared by others."""
        super(Shared, self).__init__(None, items, **defaults)
        self.is_shared = True
        self.generate_rst = False


crud_shared_options = ('crud', {'type': 'mongoc_crud_opts_t'})

ordered_option = ('ordered', {
    'type': 'bool',
    'help': 'set to ``false`` to attempt to insert all documents, continuing after errors.'
})

validate_option = ('validate', {
    'type': 'bson_validate_flags_t',
    'convert': '_mongoc_convert_validate_flags',
    'help': 'Construct a bitwise-or of all desired :symbol:`bson_validate_flags_t <bson_validate_with_error>`. Set to ``false`` to skip client-side validation of the provided BSON documents.'
})

collation_option = ('collation', {
    'type': 'document',
    'help': 'Configure textual comparisons. See :ref:`Setting Collation Order <setting_collation_order>`, and `the MongoDB Manual entry on Collation <https://docs.mongodb.com/manual/reference/collation/>`_.'
})

array_filters_option = ('arrayFilters', {
    'type': 'array',
    'help': 'An array of filters specifying to which array elements an update should apply.',
})

upsert_option = ('upsert', {
    'type': 'bool',
    'help': 'When true, creates a new document if no document matches the query.'
})

bypass_option = ('bypassDocumentValidation', {
    'type': 'mongoc_write_bypass_document_validation_t',
    'field': 'bypass',
    'help': 'Set to ``true`` to skip server-side schema validation of the provided BSON documents.'
})

opts_structs = OrderedDict([
    ('mongoc_find_one_opts_t', Struct('mongoc_collection_t', [
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
    ], generate_rst=False)),

    ('mongoc_crud_opts_t', Shared([
        ('writeConcern', {
            'type': 'mongoc_write_concern_t *',
            'convert': '_mongoc_convert_write_concern',
            'help': 'Construct a :symbol:`mongoc_write_concern_t` and use :symbol:`mongoc_write_concern_append` to add the write concern to ``opts``. See the example code for :symbol:`mongoc_client_write_command_with_opts`.'
        }),
        ('write_concern_owned', {
            'type': 'bool',
            'hidden': True,
        }),
        ('sessionId', {
            'type': 'mongoc_client_session_t *',
            'convert': '_mongoc_convert_session_id',
            'field': 'client_session',
            'help': 'Construct a :symbol:`mongoc_client_session_t` with :symbol:`mongoc_client_start_session` and use :symbol:`mongoc_client_session_append` to add the session to ``opts``. See the example code for :symbol:`mongoc_client_session_t`.'
        }),
        validate_option,
    ])),

    ('mongoc_insert_one_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        bypass_option
    ], validate='_mongoc_default_insert_vflags')),

    ('mongoc_insert_many_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        ordered_option,
        bypass_option,
    ], validate='_mongoc_default_insert_vflags', ordered='true')),

    ('mongoc_delete_one_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        bypass_option,
        collation_option,
    ])),

    ('mongoc_delete_many_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        collation_option,
    ])),

    ('mongoc_update_one_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        array_filters_option,
        upsert_option,
        bypass_option,
        collation_option,
    ], validate='_mongoc_default_update_vflags')),

    ('mongoc_update_many_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        array_filters_option,
        upsert_option,
        bypass_option,
        collation_option,
    ], validate='_mongoc_default_update_vflags')),

    ('mongoc_replace_one_opts_t', Struct('mongoc_collection_t', [
        crud_shared_options,
        bypass_option,
        collation_option,
    ], validate='_mongoc_default_replace_vflags')),
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
            sub_struct = opts_structs[the_type]
            for path, sub_option_name, sub_info in paths(sub_struct):
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


def document_opts(struct, f):
    for option_name, info in struct.items():
        if info.get('hidden'):
            continue

        the_type = info['type']
        if the_type in opts_structs:
            # E.g., the type is mongoc_crud_opts_t. Recurse.
            document_opts(opts_structs[the_type], f)
            continue

        assert 'help' in info, "No 'help' for '%s'" % option_name
        f.write("* ``{option_name}``: {info[help]}\n".format(**locals()))


for struct_name, struct in opts_structs.items():
    if not struct.generate_rst:
        continue

    name = re.sub(r'mongoc_(\w+)_t', r'\1', struct_name).replace('_', '-')
    file_name = name + '.txt'
    print(file_name)
    f = open(joinpath(doc_includes, file_name), 'w')
    f.write(
        "``opts`` may be NULL or a BSON document with additional command options:\n\n")
    document_opts(struct, f)

    f.close()
