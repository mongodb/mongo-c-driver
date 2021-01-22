# Copyright 2018-present MongoDB, Inc.
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

from collections import OrderedDict as OD
from textwrap import dedent

from evergreen_config_generator import ConfigObject


def func(func_name, **kwargs):
    od = OD([('func', func_name)])
    if kwargs:
        od['vars'] = OD(sorted(kwargs.items()))

    return od


def bootstrap(VERSION='latest', TOPOLOGY=None, **kwargs):
    if TOPOLOGY:
        return func('bootstrap mongo-orchestration',
                    VERSION=VERSION,
                    TOPOLOGY=TOPOLOGY,
                    **kwargs)

    return func('bootstrap mongo-orchestration',
                VERSION=VERSION,
                **kwargs)


def run_tests(URI=None, **kwargs):
    if URI:
        return func('run tests', URI=URI, **kwargs)

    return func('run tests', **kwargs)


def s3_put(remote_file, project_path=True, **kwargs):
    if project_path:
        remote_file = '${project}/' + remote_file

    od = OD([
        ('command', 's3.put'),
        ('params', OD([
            ('aws_key', '${aws_key}'),
            ('aws_secret', '${aws_secret}'),
            ('remote_file', remote_file),
            ('bucket', 'mciuploads'),
            ('permissions', 'public-read')]))])

    od['params'].update(kwargs)
    return od


def strip_lines(s):
    return '\n'.join(line for line in s.split('\n') if line.strip())


def shell_exec(script, test=True, errexit=True, xtrace=True, silent=False,
               continue_on_err=False, working_dir=None, background=False):
    dedented = ''
    if errexit:
        dedented += 'set -o errexit\n'

    if xtrace:
        dedented += 'set -o xtrace\n'

    dedented += dedent(strip_lines(script))
    command = OD([('command', 'shell.exec')])
    if test:
        command['type'] = 'test'

    command['params'] = OD()
    if silent:
        command['params']['silent'] = True

    if working_dir is not None:
        command['params']['working_dir'] = working_dir

    if continue_on_err:
        command['params']['continue_on_err'] = True

    if background:
        command['params']['background'] = True

    command['params']['shell'] = 'bash'
    command['params']['script'] = dedented
    return command


def targz_pack(target, source_dir, *include):
    return OD([
        ('command', 'archive.targz_pack'),
        ('params', OD([
            ('target', target),
            ('source_dir', source_dir),
            ('include', list(include))]))])


class Function(ConfigObject):
    def __init__(self, *commands):
        super(Function, self).__init__()
        self.commands = commands

    def to_dict(self):
        return list(self.commands)
