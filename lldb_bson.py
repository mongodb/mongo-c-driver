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

# -*- coding: utf-8 -*-

from collections import OrderedDict
import optparse
import shlex
import struct
import sys

import lldb

try:
    import bson
    from bson import json_util

    # WARNING: horrific hack to make DuplicateKeyDict work.
    json_util.SON = OrderedDict
except ImportError:
    pass


FLAGS = OrderedDict([
    ('INLINE', 1 << 0),
    ('STATIC', 1 << 1),
    ('RDONLY', 1 << 2),
    ('CHILD', 1 << 3),
    ('IN_CHILD', 1 << 4),
    ('NO_FREE', 1 << 5),
])

ALL_FLAGS = (1 << 6) - 1


def flags_str(flags):
    if flags == 0:
        return 'flags=0'

    return 'flags=' + '|'.join(
        name for name, value in FLAGS.items() if flags & value)


if sys.version_info[0] == 3:
    string_types = (bytes, str)
else:
    string_types = (str, unicode)


class Key(str):
    def __repr__(self):
        return "Key(%s)" % super(Key, self).__repr__()

    def __hash__(self):
        return id(self)


class DuplicateKeyDict(OrderedDict):
    """Allows duplicate keys in dicts."""

    def __setitem__(self, key, value):
        if isinstance(key, string_types):
            key = Key(key)

        super(DuplicateKeyDict, self).__setitem__(key, value)


def bson_dumps(raw_bson, oneline):
    if not bson:
        return "No PyMongo, do `python -m pip install pymongo`"

    codec_options = bson.CodecOptions(document_class=DuplicateKeyDict)
    if oneline:
        indent = None
    else:
        indent = 2

    return json_util.dumps(bson.BSON(raw_bson).decode(codec_options),
                           indent=indent)


_UNPACK_INT = struct.Struct("<i").unpack


def check(error):
    if not error.success:
        raise Exception(str(error))


def get_inline_bytes(data):
    error = lldb.SBError()
    length = data.GetData().GetSignedInt32(error, 0)
    check(error)
    return b''.join(chr(b) for b in data.GetData().uint8[:length])


def get_allocated_bytes(buf, offset, debugger):
    # I don't know why this must be so different from get_inline_bytes.
    error = lldb.SBError()
    buf_addr = buf.Dereference().GetAddress().offset + offset
    process = debugger.GetSelectedTarget().process
    len_bytes = process.ReadMemory(buf_addr, 4, error)
    check(error)
    length = _UNPACK_INT(len_bytes)[0]
    return process.ReadMemory(buf_addr, length, error)


def get_cstring(buf, offset, max_length, debugger):
    error = lldb.SBError()
    buf_addr = buf.Dereference().GetAddress().offset + offset
    process = debugger.GetSelectedTarget().process
    cstring = process.ReadCStringFromMemory(buf_addr, max_length, error)
    check(error)
    return cstring


def bson_as_json(value, debugger, verbose=False, oneline=True, raw=False):
    try:
        target = debugger.GetSelectedTarget()
        inline_t = target.FindFirstType('bson_impl_inline_t')
        alloc_t = target.FindFirstType('bson_impl_alloc_t')

        if not inline_t.GetDisplayTypeName():
            return "error: libbson not compiled with debug symbols"

        if value.TypeIsPointerType():
            value = value.Dereference()

        length = value.GetChildMemberWithName('len').GetValueAsUnsigned()
        flags = value.GetChildMemberWithName('flags').GetValueAsUnsigned()

        if flags & ~ALL_FLAGS or length < 5 or length > 16 * 1024 * 1024:
            return 'uninitialized'

        if flags & FLAGS['INLINE']:
            if length > 120:
                return 'uninitialized'

            inline = value.Cast(inline_t)
            data = inline.GetChildMemberWithName('data')
            raw_bson = get_inline_bytes(data)
        else:
            alloc = value.Cast(alloc_t)
            offset = alloc.GetChildMemberWithName('offset').GetValueAsUnsigned()
            buf = alloc.GetChildMemberWithName('buf').Dereference()
            raw_bson = get_allocated_bytes(buf, offset, debugger)

        if raw:
            return repr(raw_bson)

        ret = ''
        if verbose:
            ret += 'len=%s\n' % length
            ret += flags_str(flags) + '\n'

        ret += bson_dumps(raw_bson, oneline)
        return ret
    except Exception as exc:
        return str(exc)


class OptionParserNoExit(optparse.OptionParser):
    def exit(self, status=0, msg=None):
        raise Exception(msg)


def bson_as_json_options():
    usage = "usage: %prog [options] VARIABLE"
    description = '''Prints a libbson bson_t struct as JSON'''
    parser = OptionParserNoExit(description=description, prog='bson',
                                usage=usage,
                                add_help_option=False)
    parser.add_option('-v', '--verbose', action='store_true',
                      help='Print length and flags of bson_t.')
    parser.add_option('-1', '--one-line', action='store_true',
                      dest='oneline', help="Don't indent JSON")
    parser.add_option('-r', '--raw', action='store_true',
                      help='Print byte string, not JSON')
    parser.add_option('-h', '--help', action='store_true',
                      help='Print help and exit')

    return parser


def bson_as_json_command(debugger, command, result, internal_dict):
    command_args = shlex.split(command)
    parser = bson_as_json_options()

    try:
        options, args = parser.parse_args(command_args)
    except Exception as exc:
        result.AppendMessage(str(exc))
        return

    if options.help or not args:
        result.AppendMessage(parser.format_help())
        return

    process = debugger.GetSelectedTarget().GetProcess()
    frame = process.GetSelectedThread().GetFrameAtIndex(0)

    for arg in args:
        value = frame.FindVariable(arg)
        result.AppendMessage(
            bson_as_json(value,
                         debugger,
                         verbose=options.verbose,
                         oneline=options.oneline,
                         raw=options.raw))


def bson_type_summary(value, internal_dict):
    return bson_as_json(value, lldb.debugger)


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        'type summary add -F lldb_bson.bson_type_summary bson_t')

    debugger.HandleCommand(
        'command script add --help \"%s\"'
        ' -f lldb_bson.bson_as_json_command bson' %
        bson_as_json_options().format_help().replace('"', "'"))

    debugger.GetErrorFileHandle().write('"bson" command installed by lldb_bson.py\n')
