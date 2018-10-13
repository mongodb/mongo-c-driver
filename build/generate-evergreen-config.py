#!/usr/bin/env python
#
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

"""Generate C Driver's config.yml for Evergreen testing.

We find that generating configuration from Python data structures and a template
file is more legible than Evergreen's matrix syntax or a handwritten file.

Written for Python 2.6+, requires Jinja 2 for templating.
"""

import sys
from collections import namedtuple, OrderedDict as OD
from itertools import product
from os.path import dirname, join as joinpath, normpath
from textwrap import dedent
from types import NoneType

try:
    # Python 3 abstract base classes.
    import collections.abc as abc
except ImportError:
    import collections as abc

try:
    import yaml
    import yamlordereddictloader
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    sys.stderr.write("try 'pip install -r build/requirements.txt'")
    raise

this_dir = dirname(__file__)
evergreen_dir = normpath(joinpath(this_dir, '../.evergreen'))


# We want legible YAML tasks:
#
#     - name: debug-compile
#       tags: [zlib, snappy, compression, openssl]
#       commands:
#       - command: shell.exec
#         params:
#           script: |-
#             set -o errexit
#             set -o xtrace
#             ...
#
# Write values compactly except multiline strings, which use "|" style. Write
# tag sets as lists.

class Dumper(yamlordereddictloader.Dumper):
    def __init__(self, *args, **kwargs):
        super(Dumper, self).__init__(*args, **kwargs)
        self.add_representer(set, type(self).represent_set)

    def represent_scalar(self, tag, value, style=None):
        if isinstance(value, (str, unicode)) and '\n' in value:
            style = '|'
        return super(Dumper, self).represent_scalar(tag, value, style)

    def represent_set(self, data):
        return super(Dumper, self).represent_list(sorted(data))


def func(func_name, **kwargs):
    od = OD([('func', func_name)])
    if kwargs:
        od['vars'] = OD(sorted(kwargs.items()))

    return od


def strip_lines(s):
    return '\n'.join(line for line in s.split('\n') if line.strip())


def shell_exec(script):
    return OD([
        ('command', 'shell.exec'),
        ('type', 'test'),
        ('params', OD([('working_dir', 'mongoc'),
                       ('script', dedent(strip_lines(script)))])),
    ])


def s3_put(local_file, remote_file, content_type, display_name=None):
    od = OD([
        ('command', 's3.put'),
        ('params', OD([
            ('aws_key', '${aws_key}'),
            ('aws_secret', '${aws_secret}'),
            ('local_file', local_file),
            ('remote_file', '${project}/' + remote_file),
            ('bucket', 'mciuploads'),
            ('permissions', 'public-read'),
            ('content_type', content_type)]))])

    if display_name is not None:
        od['params']['display_name'] = display_name

    return od


class Task(object):
    def __init__(self, *args, **kwargs):
        super(Task, self).__init__()
        self.tags = set()
        self.options = OD()
        self.depends_on = kwargs.pop('depends_on', None)
        self.commands = kwargs.pop('commands', None)
        assert (isinstance(self.commands, (abc.Sequence, NoneType)))

    name_prefix = 'test'

    @property
    def name(self):
        return 'UNSET'

    def add_tags(self, *args):
        self.tags.update(args)

    def has_tags(self, *args):
        return bool(self.tags.intersection(args))

    def add_dependency(self, dependency):
        if not isinstance(dependency, abc.Mapping):
            dependency = OD([('name', dependency)])

        if self.depends_on is None:
            self.depends_on = dependency
        elif isinstance(self.depends_on, abc.Mapping):
            self.depends_on = [self.depends_on, dependency]
        else:
            self.depends_on.append(dependency)

    def display(self, axis_name):
        value = getattr(self, axis_name)
        # E.g., if self.auth is False, return 'noauth'.
        if value is False:
            return 'no' + axis_name

        if value is True:
            return axis_name

        return value

    def on_off(self, *args, **kwargs):
        assert not (args and kwargs)
        if args:
            axis_name, = args
            return 'on' if getattr(self, axis_name) else 'off'

        (axis_name, value), = kwargs
        return 'on' if getattr(self, axis_name) == value else 'off'

    def to_dict(self):
        task = OD([('name', self.name)])
        if self.tags:
            task['tags'] = self.tags
        task.update(self.options)
        if self.depends_on:
            task['depends_on'] = self.depends_on
        task['commands'] = self.commands or []
        return task

    def to_yaml(self):
        return yaml.dump(self.to_dict(), Dumper=Dumper)


class NamedTask(Task):
    def __init__(self, task_name, commands=None, **kwargs):
        super(NamedTask, self).__init__(commands=commands, **kwargs)
        self._task_name = task_name

    @property
    def name(self):
        return self._task_name


class FuncTask(NamedTask):
    def __init__(self, task_name, *args, **kwargs):
        commands = [func(func_name) for func_name in args]
        super(FuncTask, self).__init__(task_name, commands=commands, **kwargs)


class CompileTask(NamedTask):
    def __init__(self, task_name, tags=None, config='debug',
                 compression='default', continue_on_err=False,
                 extra_commands=None, depends_on=None, **kwargs):
        super(CompileTask, self).__init__(task_name=task_name,
                                          depends_on=depends_on,
                                          **kwargs)
        if tags:
            self.add_tags(*tags)

        self.extra_commands = extra_commands or []

        # Environment variables for .evergreen/compile.sh.
        self.compile_sh_opt = kwargs
        if config == 'debug':
            self.compile_sh_opt['DEBUG'] = 'ON'
        else:
            assert config == 'release'
            self.compile_sh_opt['RELEASE'] = 'ON'

        if compression != 'default':
            self.compile_sh_opt['SNAPPY'] = (
                'ON' if compression in ('all', 'snappy') else 'OFF')
            self.compile_sh_opt['ZLIB'] = (
                'BUNDLED' if compression in ('all', 'zlib') else 'OFF')

        self.continue_on_err = continue_on_err

    def to_dict(self):
        task = super(CompileTask, self).to_dict()

        script = "set -o errexit\nset -o xtrace\n"
        for opt, value in sorted(self.compile_sh_opt.items()):
            script += 'export %s="%s"\n' % (opt, value)

        script += "CC='${CC}' MARCH='${MARCH}' sh .evergreen/compile.sh"
        task['commands'].append(shell_exec(script))
        task['commands'].append(func('upload build'))
        task['commands'].extend(self.extra_commands)
        return task


class SpecialTask(CompileTask):
    def __init__(self, *args, **kwargs):
        super(SpecialTask, self).__init__(*args, **kwargs)
        self.add_tags('special')


class LinkTask(NamedTask):
    def __init__(self, task_name, extra_commands, orchestration=True, **kwargs):
        if orchestration:
            vars = OD([('VERSION', 'latest')])
            if orchestration == 'ssl':
                vars['SSL'] = 1
            bootstrap_commands = [func('bootstrap mongo-orchestration', **vars)]
        else:
            bootstrap_commands = []

        super(LinkTask, self).__init__(
            task_name=task_name,
            depends_on=OD([('name', 'make-release-archive'),
                           ('variant', 'releng')]),
            commands=bootstrap_commands + extra_commands,
            **kwargs)


compile_tasks = [
    NamedTask('check-public-headers',
              commands=[shell_exec('sh ./.evergreen/check-public-headers.sh')]),
    FuncTask('make-release-archive',
             'release archive', 'upload docs', 'upload man pages',
             'upload release', 'upload build'),
    CompileTask('hardened-compile',
                tags=['hardened'],
                compression=None,
                CFLAGS='-fno-strict-overflow -D_FORTIFY_SOURCE=2 -fstack-protector-all -fPIE -O',
                LDFLAGS='-pie -Wl,-z,relro -Wl,-z,now'),
    FuncTask('abi-compliance-check', 'abi report'),
    CompileTask('debug-compile-compression-zlib',
                tags=['zlib', 'compression'],
                compression='zlib'),
    CompileTask('debug-compile-compression-snappy',
                tags=['snappy', 'compression'],
                compression='snappy'),
    CompileTask('debug-compile-compression',
                tags=['zlib', 'snappy', 'compression'],
                compression='all'),
    CompileTask('debug-compile-no-align',
                tags=['debug-compile'],
                compression='zlib',
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF"),
    CompileTask('debug-compile-nosasl-nossl',
                tags=['debug-compile', 'nosasl', 'nossl']),
    CompileTask('debug-compile-lto', CFLAGS='-flto'),
    CompileTask('debug-compile-lto-thin', CFLAGS='-flto=thin'),
    SpecialTask('debug-compile-c11',
                tags=['debug-compile', 'c11', 'stdflags'],
                CFLAGS='-std=c11 -D_XOPEN_SOURCE=600'),
    SpecialTask('debug-compile-c99',
                tags=['debug-compile', 'c99', 'stdflags'],
                CFLAGS='-std=c99 -D_XOPEN_SOURCE=600'),
    SpecialTask('debug-compile-c89',
                tags=['debug-compile', 'c89', 'stdflags'],
                CFLAGS='-std=c89 -D_POSIX_C_SOURCE=200112L -pedantic'),
    SpecialTask('debug-compile-valgrind',
                tags=['debug-compile', 'valgrind'],
                SASL='OFF',
                SSL='OPENSSL',
                VALGRIND='ON',
                CFLAGS='-DBSON_MEMCHECK'),
    SpecialTask('debug-compile-coverage',
                tags=['debug-compile', 'coverage'],
                COVERAGE='ON',
                extra_commands=[func('upload coverage')]),
    CompileTask('debug-compile-no-counters',
                tags=['debug-compile', 'no-counters'],
                ENABLE_SHM_COUNTERS='OFF'),
    SpecialTask('debug-compile-asan-clang',
                tags=['debug-compile', 'asan-clang'],
                compression='zlib',
                CC='clang-3.8',
                CFLAGS='-fsanitize=address -fno-omit-frame-pointer'
                       ' -DBSON_MEMCHECK',
                CHECK_LOG='ON',
                EXTRA_CONFIGURE_FLAGS='-DENABLE_EXTRA_ALIGNMENT=OFF',
                PATH='/usr/lib/llvm-3.8/bin:$PATH'),
    # include -pthread in CFLAGS on gcc to address the issue explained here:
    # https://groups.google.com/forum/#!topic/address-sanitizer/JxnwgrWOLuc
    SpecialTask('debug-compile-asan-gcc',
                compression='zlib',
                CFLAGS='-fsanitize=address -pthread',
                CHECK_LOG='ON',
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF"),
    SpecialTask('debug-compile-asan-clang-openssl',
                tags=['debug-compile', 'asan-clang'],
                compression='zlib',
                CC='clang-3.8',
                CFLAGS='-fsanitize=address -fno-omit-frame-pointer'
                       ' -DBSON_MEMCHECK',
                CHECK_LOG='ON',
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF",
                PATH='/usr/lib/llvm-3.8/bin:$PATH',
                SSL='OPENSSL'),
    SpecialTask('debug-compile-ubsan',
                compression='zlib',
                CC='clang-3.8',
                CFLAGS='-fsanitize=undefined -fno-omit-frame-pointer'
                       ' -DBSON_MEMCHECK',
                CHECK_LOG='ON',
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF",
                PATH='/usr/lib/llvm-3.8/bin:$PATH'),
    SpecialTask('debug-compile-scan-build',
                tags=['clang', 'debug-compile', 'scan-build'],
                continue_on_err=True,
                ANALYZE='ON',
                CC='clang',
                extra_commands=[
                    func('upload scan artifacts'),
                    shell_exec('''
                        if find scan -name \*.html | grep -q html; then
                          exit 123
                        fi''')]),
    CompileTask('compile-tracing',
                TRACING='ON'),
    CompileTask('release-compile',
                config='release',
                depends_on=[OD([('name', 'make-release-archive'),
                                ('variant', 'releng')])]),
    CompileTask('debug-compile-nosasl-openssl',
                tags=['debug-compile', 'nosasl', 'openssl'],
                SSL='OPENSSL'),
    CompileTask('debug-compile-nosasl-darwinssl',
                tags=['debug-compile', 'nosasl', 'darwinssl'],
                SSL='DARWIN'),
    CompileTask('debug-compile-nosasl-winssl',
                tags=['debug-compile', 'nosasl', 'winssl'],
                SSL='WINDOWS'),
    CompileTask('debug-compile-sasl-nossl',
                tags=['debug-compile', 'sasl', 'nossl'],
                SASL='AUTO',
                SSL='OFF'),
    CompileTask('debug-compile-sasl-openssl',
                tags=['debug-compile', 'sasl', 'openssl'],
                SASL='AUTO',
                SSL='OPENSSL'),
    CompileTask('debug-compile-sasl-darwinssl',
                tags=['debug-compile', 'sasl', 'darwinssl'],
                SASL='AUTO',
                SSL='DARWIN'),
    CompileTask('debug-compile-sasl-winssl',
                tags=['debug-compile', 'sasl', 'winssl'],
                SASL='AUTO',
                SSL='WINDOWS'),
    CompileTask('debug-compile-sspi-nossl',
                tags=['debug-compile', 'sspi', 'nossl'],
                SASL='SSPI',
                SSL='OFF'),
    CompileTask('debug-compile-sspi-openssl',
                tags=['debug-compile', 'sspi', 'openssl'],
                SASL='SSPI',
                SSL='OPENSSL'),
    CompileTask('debug-compile-rdtscp',
                ENABLE_RDTSCP='ON'),
    CompileTask('debug-compile-sspi-winssl',
                tags=['debug-compile', 'sspi', 'winssl'],
                SASL='SSPI',
                SSL='WINDOWS'),
    CompileTask('debug-compile-nosrv',
                tags=['debug-compile'],
                SRV='OFF'),
    LinkTask('link-with-cmake',
             extra_commands=[
                 func('link sample program', BUILD_SAMPLE_WITH_CMAKE=1)]),
    LinkTask('link-with-cmake-ssl',
             extra_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      ENABLE_SSL=1)]),
    LinkTask('link-with-cmake-snappy',
             extra_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      ENABLE_SNAPPY=1)]),
    LinkTask('link-with-cmake-mac',
             extra_commands=[
                 func('link sample program', BUILD_SAMPLE_WITH_CMAKE=1)]),
    LinkTask('link-with-cmake-windows',
             extra_commands=[func('link sample program MSVC')]),
    LinkTask('link-with-cmake-windows-ssl',
             extra_commands=[func('link sample program MSVC', ENABLE_SSL=1)],
             orchestration='ssl'),
    LinkTask('link-with-cmake-windows-snappy',
             extra_commands=[
                 func('link sample program MSVC', ENABLE_SNAPPY=1)]),
    LinkTask('link-with-cmake-mingw',
             extra_commands=[func('link sample program mingw')]),
    LinkTask('link-with-pkg-config',
             extra_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-mac',
             extra_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-ssl',
             extra_commands=[func('link sample program', ENABLE_SSL=1)]),
    LinkTask('link-with-bson',
             extra_commands=[func('link sample program bson')],
             orchestration=False),
    LinkTask('link-with-bson-mac',
             extra_commands=[func('link sample program bson')],
             orchestration=False),
    LinkTask('link-with-bson-windows',
             extra_commands=[func('link sample program MSVC bson')],
             orchestration=False),
    LinkTask('link-with-bson-mingw',
             extra_commands=[func('link sample program mingw bson')],
             orchestration=False),
    NamedTask('debian-package-build',
              commands=[
                  shell_exec('export IS_PATCH="${is_patch}"\n'
                             'sh .evergreen/debian_package_build.sh'),
                  s3_put('deb.tar.gz',
                         '${branch_name}/mongo-c-driver-debian-packages-${CURRENT_VERSION}.tar.gz',
                         '${content_type|application/x-gzip}')]),
    NamedTask('install-uninstall-check-mingw',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_exec(r'''
                  set -o xtrace
                  set -o errexit
                  export CC="C:/mingw-w64/x86_64-4.9.1-posix-seh-rt_v3-rev1/mingw64/bin/gcc.exe"
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check-msvc',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_exec(r'''
                  set -o xtrace
                  set -o errexit
                  export CC="Visual Studio 14 2015 Win64"
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_exec(r'''
                  set -o xtrace
                  set -o errexit
                  DESTDIR="$(pwd)/dest" sh ./.evergreen/install-uninstall-check.sh
                  BSON_ONLY=1 sh ./.evergreen/install-uninstall-check.sh
                  sh ./.evergreen/install-uninstall-check.sh''')]),
]

integration_task_axes = OD([
    ('valgrind', ['valgrind', False]),
    ('asan', ['asan', False]),
    ('coverage', ['coverage', False]),
    ('version', ['latest', '4.0', '3.6', '3.4', '3.2', '3.0']),
    ('topology', ['server', 'replica_set', 'sharded_cluster']),
    ('auth', [True, False]),
    ('sasl', ['sasl', 'sspi', False]),
    ('ssl', ['openssl', 'darwinssl', 'winssl', False]),
])


class IntegrationTask(Task, namedtuple('Task', tuple(integration_task_axes))):
    @property
    def name(self):
        def name_part(axis_name):
            part = self.display(axis_name)
            if part == 'replica_set':
                return 'replica-set'
            elif part == 'sharded_cluster':
                return 'sharded'
            return part

        return self.name_prefix + '-' + '-'.join(
            name_part(axis_name) for axis_name in integration_task_axes
            if getattr(self, axis_name) or axis_name in ('auth', 'sasl', 'ssl'))

    def to_dict(self):
        task = super(IntegrationTask, self).to_dict()
        commands = task['commands']
        if self.depends_on:
            commands.append(
                func('fetch build', BUILD_NAME=self.depends_on['name']))
        if self.coverage:
            commands.append(func('debug-compile-coverage-notest-%s-%s' % (
                self.display('sasl'), self.display('ssl')
            )))
        commands.append(func('bootstrap mongo-orchestration',
                             VERSION=self.version,
                             TOPOLOGY=self.topology,
                             AUTH='auth' if self.auth else 'noauth',
                             SSL=self.display('ssl')))
        commands.append(func('run tests',
                             VALGRIND=self.on_off('valgrind'),
                             ASAN=self.on_off('asan'),
                             AUTH=self.display('auth'),
                             SSL=self.display('ssl')))
        if self.coverage:
            commands.append(func('update codecov.io'))

        return task


auth_task_axes = OD([
    ('sasl', ['sasl', 'sspi', False]),
    ('ssl', ['openssl', 'darwinssl', 'winssl']),
])


class AuthTask(Task, namedtuple('Task', tuple(auth_task_axes))):
    name_prefix = 'authentication-tests'

    @property
    def name(self):
        rv = self.name_prefix + '-' + self.display('ssl')
        if self.sasl:
            return rv
        else:
            return rv + '-nosasl'

    def to_dict(self):
        task = super(AuthTask, self).to_dict()
        task['commands'].append(func('fetch build',
                                     BUILD_NAME=self.depends_on['name']))
        task['commands'].append(func('run auth tests'))
        return task


def matrix(cell_class, axes):
    return set(cell_class(*cell) for cell in product(*axes.values()))


class Prohibited(Exception):
    pass


def require(rule):
    if not rule:
        raise Prohibited()


def prohibit(rule):
    if rule:
        raise Prohibited()


def both_or_neither(rule0, rule1):
    if rule0:
        require(rule1)
    else:
        prohibit(rule1)


def allow_integration_test_task(task):
    if task.valgrind:
        prohibit(task.asan)
        prohibit(task.sasl)
        require(task.ssl in ('openssl', False))
        prohibit(task.coverage)
        # Valgrind only with auth+SSL or no auth + no SSL.
        if task.auth:
            require(task.ssl == 'openssl')
        else:
            prohibit(task.ssl)

    if task.auth:
        require(task.ssl)

    if task.sasl == 'sspi':
        # Only one task.
        require(task.topology == 'server')
        require(task.version == 'latest')
        require(task.ssl == 'winssl')
        require(task.auth)

    if not task.ssl:
        prohibit(task.sasl)

    if task.coverage:
        prohibit(task.sasl)

        if task.auth:
            require(task.ssl == 'openssl')
        else:
            prohibit(task.ssl)

    if task.asan:
        prohibit(task.sasl)
        prohibit(task.coverage)

        # Address sanitizer only with auth+SSL or no auth + no SSL.
        if task.auth:
            require(task.ssl == 'openssl')
        else:
            prohibit(task.ssl)


def make_integration_test_tasks():
    tasks_list = []
    for task in matrix(IntegrationTask, integration_task_axes):
        try:
            allow_integration_test_task(task)
        except Prohibited:
            continue

        if task.valgrind:
            task.add_tags('test-valgrind')
            task.options['exec_timeout_secs'] = 7200
        elif task.coverage:
            task.add_tags('test-coverage')
            task.options['exec_timeout_secs'] = 3600
        elif task.asan:
            task.add_tags('test-asan')
            task.options['exec_timeout_secs'] = 3600
        else:
            task.add_tags(task.topology,
                          task.version,
                          task.display('ssl'),
                          task.display('sasl'),
                          task.display('auth'))

        # E.g., test-latest-server-auth-sasl-ssl needs debug-compile-sasl-ssl.
        # Coverage tasks use a build function instead of depending on a task.
        if task.valgrind:
            task.add_dependency('debug-compile-valgrind')
        elif task.asan and task.ssl:
            task.add_dependency('debug-compile-asan-clang-%s' % (
                task.display('ssl'),))
        elif task.asan:
            assert not task.sasl
            task.add_dependency('debug-compile-asan-clang')
        elif not task.coverage:
            task.add_dependency('debug-compile-%s-%s' % (
                task.display('sasl'), task.display('ssl')))

        tasks_list.append(task)

    return tasks_list


def allow_auth_test_task(task):
    both_or_neither(task.ssl == 'winssl', task.sasl == 'sspi')
    if not task.sasl:
        require(task.ssl == 'openssl')


def make_auth_test_tasks():
    tasks_list = []
    for task in matrix(AuthTask, auth_task_axes):
        try:
            allow_auth_test_task(task)
        except Prohibited:
            continue

        task.add_tags('authentication-tests',
                      task.display('ssl'),
                      task.display('sasl'))

        task.add_dependency('debug-compile-%s-%s' % (
            task.display('sasl'), task.display('ssl')))

        tasks_list.append(task)

    return tasks_list


env = Environment(loader=FileSystemLoader(this_dir),
                  trim_blocks=True,
                  lstrip_blocks=True,
                  extensions=['jinja2.ext.loopcontrols'])

env.filters['tag_list'] = lambda value: (
        '[' + ', '.join('"%s"' % (tag,) for tag in value) + ']')

print('.evergreen/config.yml')
f = open(joinpath(evergreen_dir, 'config.yml'), 'w+')
t = env.get_template('config.yml.template')
f.write(t.render(globals()))
f.write('\n')
