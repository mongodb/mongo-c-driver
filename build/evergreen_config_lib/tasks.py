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
from itertools import chain

try:
    # Python 3 abstract base classes.
    import collections.abc as abc
except ImportError:
    import collections as abc

from evergreen_config_generator.functions import (
    bootstrap, func, run_tests, s3_put)
from evergreen_config_generator.tasks import (
    both_or_neither, FuncTask, MatrixTask, NamedTask, prohibit, require, Task)
from evergreen_config_lib import shell_mongoc
from pkg_resources import parse_version


class CompileTask(NamedTask):
    def __init__(self, task_name, tags=None, config='debug',
                 compression='default', continue_on_err=False,
                 suffix_commands=None, depends_on=None,
                 extra_script=None, prefix_commands=None, **kwargs):
        super(CompileTask, self).__init__(task_name=task_name,
                                          depends_on=depends_on,
                                          tags=tags,
                                          **kwargs)

        self.suffix_commands = suffix_commands or []
        self.prefix_commands = prefix_commands or []
        if extra_script:
            self.extra_script = "\n" + extra_script
        else:
            self.extra_script = ""

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
            self.compile_sh_opt['ZSTD'] = (
                'ON' if compression in ('all', 'zstd') else 'OFF')

        self.continue_on_err = continue_on_err

    def to_dict(self):
        task = super(CompileTask, self).to_dict()

        task['commands'].extend(self.prefix_commands)

        script = ''
        for opt, value in sorted(self.compile_sh_opt.items()):
            script += 'export %s="%s"\n' % (opt, value)

        script += "CC='${CC}' MARCH='${MARCH}' sh .evergreen/compile.sh" + \
                  self.extra_script
        task['commands'].append(shell_mongoc(script))
        task['commands'].append(func('upload build'))
        task['commands'].extend(self.suffix_commands)
        return task


class SpecialTask(CompileTask):
    def __init__(self, *args, **kwargs):
        super(SpecialTask, self).__init__(*args, **kwargs)
        self.add_tags('special')


class CompileWithClientSideEncryption(CompileTask):
    def __init__(self, *args, **kwargs):
        # Compiling with ClientSideEncryption support is a little strange.
        # It requires linking against the library libmongocrypt. But libmongocrypt
        # depends on libbson. So we do the following:
        # 1. Build and install libbson normally
        # 2. Build and install libmongocrypt (which links against the previously built libbson)
        # 3. Build and install libmongoc
        # First, compile and install without CSE.
        # Then, compile and install libmongocrypt.
        compile_with_cse = CompileTask(*args,
                                       CFLAGS="-fPIC",
                                       COMPILE_LIBMONGOCRYPT="ON",
                                       EXTRA_CONFIGURE_FLAGS="-DENABLE_CLIENT_SIDE_ENCRYPTION=ON",
                                       **kwargs).to_dict()
        extra_script = "rm CMakeCache.txt\n" + \
                       compile_with_cse["commands"][0]["params"]["script"]

        # Skip running mock server tests, because those were already run in the non-CSE build.
        super(CompileWithClientSideEncryption, self).__init__(*args, CFLAGS="-fPIC",
                                                              extra_script=extra_script,
                                                              EXTRA_CONFIGURE_FLAGS="-DENABLE_MONGOC=OFF",
                                                              SKIP_MOCK_TESTS="ON",
                                                              **kwargs)
        self.add_tags('client-side-encryption', 'special')


class CompileWithClientSideEncryptionAsan(CompileTask):
    def __init__(self, *args, **kwargs):
        compile_with_cse = CompileTask(*args,
                                       CFLAGS="-fPIC -fsanitize=address -fno-omit-frame-pointer -DBSON_MEMCHECK",
                                       COMPILE_LIBMONGOCRYPT="ON",
                                       CHECK_LOG="ON",
                                       EXTRA_CONFIGURE_FLAGS="-DENABLE_CLIENT_SIDE_ENCRYPTION=ON -DENABLE_EXTRA_ALIGNMENT=OFF",
                                       PATH='/usr/lib/llvm-3.8/bin:$PATH',
                                       **kwargs).to_dict()
        extra_script = "rm CMakeCache.txt\n" + \
                       compile_with_cse["commands"][0]["params"]["script"]

        # Skip running mock server tests, because those were already run in the non-CSE build.
        super(CompileWithClientSideEncryptionAsan, self).__init__(*args,
                                                                  CFLAGS="-fPIC -fsanitize=address -fno-omit-frame-pointer -DBSON_MEMCHECK",
                                                                  extra_script=extra_script,
                                                                  CHECK_LOG="ON",
                                                                  EXTRA_CONFIGURE_FLAGS="-DENABLE_MONGOC=OFF -DENABLE_EXTRA_ALIGNMENT=OFF",
                                                                  PATH='/usr/lib/llvm-3.8/bin:$PATH',
                                                                  SKIP_MOCK_TESTS="ON",
                                                                  **kwargs)
        self.add_tags('client-side-encryption')


class LinkTask(NamedTask):
    def __init__(self, task_name, suffix_commands, orchestration=True, **kwargs):
        if orchestration == 'ssl':
            bootstrap_commands = [bootstrap(SSL=1)]
        elif orchestration:
            bootstrap_commands = [bootstrap()]
        else:
            bootstrap_commands = []

        super(LinkTask, self).__init__(
            task_name=task_name,
            depends_on=OD([('name', 'make-release-archive'),
                           ('variant', 'releng')]),
            commands=bootstrap_commands + suffix_commands,
            **kwargs)


all_tasks = [
    NamedTask('check-headers',
              commands=[shell_mongoc('sh ./.evergreen/check-public-decls.sh'),
                        shell_mongoc('python ./.evergreen/check-preludes.py .')]),
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
    CompileTask('debug-compile-compression-zstd',
                tags=['zstd', 'compression'],
                compression='zstd'),
    CompileTask('debug-compile-compression',
                tags=['zlib', 'snappy', 'zstd', 'compression'],
                compression='all'),
    CompileTask('debug-compile-no-align',
                tags=['debug-compile'],
                compression='zlib',
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF"),
    CompileTask('debug-compile-nosasl-nossl',
                tags=['debug-compile', 'nosasl', 'nossl'],
                SSL='OFF'),
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
                suffix_commands=[func('upload coverage')]),
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
                suffix_commands=[
                    func('upload scan artifacts'),
                    shell_mongoc('''
                        if find scan -name \*.html | grep -q html; then
                          exit 123
                        fi''')]),
    CompileTask('compile-tracing',
                TRACING='ON', CFLAGS='-Werror -Wno-cast-align'),
    CompileTask('release-compile',
                config='release',
                depends_on=OD([('name', 'make-release-archive'),
                               ('variant', 'releng')])),
    CompileTask('debug-compile-nosasl-openssl',
                tags=['debug-compile', 'nosasl', 'openssl'],
                SSL='OPENSSL'),
    CompileTask('debug-compile-nosasl-openssl-static',
                tags=['debug-compile', 'nosasl', 'openssl-static'],
                SSL='OPENSSL_STATIC'),
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
    CompileTask('debug-compile-sasl-openssl-static',
                tags=['debug-compile', 'sasl', 'openssl-static'],
                SASL='AUTO',
                SSL='OPENSSL_STATIC'),
    CompileTask('debug-compile-sasl-darwinssl',
                tags=['debug-compile', 'sasl', 'darwinssl'],
                SASL='AUTO',
                SSL='DARWIN'),
    CompileTask('debug-compile-sasl-winssl',
                tags=['debug-compile', 'sasl', 'winssl'],
                # Explicitly use CYRUS.
                SASL='CYRUS',
                SSL='WINDOWS'),
    CompileTask('debug-compile-sspi-nossl',
                tags=['debug-compile', 'sspi', 'nossl'],
                SASL='SSPI',
                SSL='OFF'),
    CompileTask('debug-compile-sspi-openssl',
                tags=['debug-compile', 'sspi', 'openssl'],
                SASL='SSPI',
                SSL='OPENSSL'),
    CompileTask('debug-compile-sspi-openssl-static',
                tags=['debug-compile', 'sspi', 'openssl-static'],
                SASL='SSPI',
                SSL='OPENSSL_STATIC'),
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
             suffix_commands=[
                 func('link sample program', BUILD_SAMPLE_WITH_CMAKE=1)]),
    LinkTask('link-with-cmake-ssl',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      ENABLE_SSL=1)]),
    LinkTask('link-with-cmake-snappy',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      ENABLE_SNAPPY=1)]),
    LinkTask('link-with-cmake-mac',
             suffix_commands=[
                 func('link sample program', BUILD_SAMPLE_WITH_CMAKE=1)]),
    LinkTask('link-with-cmake-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1)]),
    LinkTask('link-with-cmake-ssl-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1,
                      ENABLE_SSL=1)]),
    LinkTask('link-with-cmake-snappy-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1,
                      ENABLE_SNAPPY=1)]),
    LinkTask('link-with-cmake-mac-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1)]),
    LinkTask('link-with-cmake-windows',
             suffix_commands=[func('link sample program MSVC')]),
    LinkTask('link-with-cmake-windows-ssl',
             suffix_commands=[func('link sample program MSVC', ENABLE_SSL=1)],
             orchestration='ssl'),
    LinkTask('link-with-cmake-windows-snappy',
             suffix_commands=[
                 func('link sample program MSVC', ENABLE_SNAPPY=1)]),
    LinkTask('link-with-cmake-mingw',
             suffix_commands=[func('link sample program mingw')]),
    LinkTask('link-with-pkg-config',
             suffix_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-mac',
             suffix_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-ssl',
             suffix_commands=[func('link sample program', ENABLE_SSL=1)]),
    LinkTask('link-with-bson',
             suffix_commands=[func('link sample program bson')],
             orchestration=False),
    LinkTask('link-with-bson-mac',
             suffix_commands=[func('link sample program bson')],
             orchestration=False),
    LinkTask('link-with-bson-windows',
             suffix_commands=[func('link sample program MSVC bson')],
             orchestration=False),
    LinkTask('link-with-bson-mingw',
             suffix_commands=[func('link sample program mingw bson')],
             orchestration=False),
    NamedTask('debian-package-build',
              commands=[
                  shell_mongoc('export IS_PATCH="${is_patch}"\n'
                               'sh .evergreen/debian_package_build.sh'),
                  s3_put(local_file='deb.tar.gz',
                         remote_file='${branch_name}/mongo-c-driver-debian-packages-${CURRENT_VERSION}.tar.gz',
                         content_type='${content_type|application/x-gzip}'),
                  s3_put(local_file='deb.tar.gz',
                         remote_file='${branch_name}/${revision}/${version_id}/${build_id}/${execution}/mongo-c-driver-debian-packages.tar.gz',
                         content_type='${content_type|application/x-gzip}')]),
    NamedTask('rpm-package-build',
              commands=[
                  shell_mongoc('sh .evergreen/build_snapshot_rpm.sh'),
                  s3_put(local_file='rpm.tar.gz',
                         remote_file='${branch_name}/mongo-c-driver-rpm-packages-${CURRENT_VERSION}.tar.gz',
                         content_type='${content_type|application/x-gzip}'),
                  s3_put(local_file='rpm.tar.gz',
                         remote_file='${branch_name}/${revision}/${version_id}/${build_id}/${execution}/mongo-c-driver-rpm-packages.tar.gz',
                         content_type='${content_type|application/x-gzip}')]),
    NamedTask('install-uninstall-check-mingw',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_mongoc(r'''
                  export CC="C:/mingw-w64/x86_64-4.9.1-posix-seh-rt_v3-rev1/mingw64/bin/gcc.exe"
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check-msvc',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_mongoc(r'''
                  export CC="Visual Studio 14 2015 Win64"
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_mongoc(r'''
                  DESTDIR="$(pwd)/dest" sh ./.evergreen/install-uninstall-check.sh
                  BSON_ONLY=1 sh ./.evergreen/install-uninstall-check.sh
                  sh ./.evergreen/install-uninstall-check.sh''')]),
    CompileTask('debug-compile-with-warnings',
                CFLAGS='-Werror -Wno-cast-align'),
    CompileWithClientSideEncryption('debug-compile-sasl-openssl-cse', tags=[
        'debug-compile', 'sasl', 'openssl'], SASL="AUTO", SSL="OPENSSL"),
    CompileWithClientSideEncryption('debug-compile-sasl-openssl-static-cse', tags=[
        'debug-compile', 'sasl', 'openssl-static'], SASL="AUTO", SSL="OPENSSL_STATIC"),
    CompileWithClientSideEncryption('debug-compile-sasl-darwinssl-cse', tags=[
        'debug-compile', 'sasl', 'darwinssl'], SASL="AUTO", SSL="DARWIN"),
    CompileWithClientSideEncryption('debug-compile-sasl-winssl-cse', tags=[
        'debug-compile', 'sasl', 'winssl'], SASL="AUTO", SSL="WINDOWS"),
    CompileWithClientSideEncryptionAsan('debug-compile-asan-openssl-cse', tags=[
        'debug-compile', 'asan-clang'], SSL="OPENSSL"),
    CompileTask('debug-compile-nosasl-openssl-1.0.1',
                prefix_commands=[func("install ssl", SSL="openssl-1.0.1u")],
                CFLAGS="-Wno-redundant-decls", SSL="OPENSSL", SASL="OFF"),
    SpecialTask('debug-compile-tsan-openssl',
                tags=['tsan'],
                CFLAGS='-fsanitize=thread -fno-omit-frame-pointer',
                CHECK_LOG='ON',
                SSL='OPENSSL',
                EXTRA_CONFIGURE_FLAGS='-DENABLE_EXTRA_ALIGNMENT=OFF -DENABLE_SHM_COUNTERS=OFF'),
    NamedTask('build-and-test-with-toolchain',
              commands=[
                  OD([('command', 's3.get'),
                      ('params', OD([
                          ('aws_key', '${toolchain_aws_key}'),
                          ('aws_secret', '${toolchain_aws_secret}'),
                          ('remote_file',
                           'mongo-c-toolchain/${distro_id}/mongo-c-toolchain.tar.gz'),
                          ('bucket', 'mongo-c-toolchain'),
                          ('local_file', 'mongo-c-toolchain.tar.gz'),
                      ]))]),
                  shell_mongoc('sh ./.evergreen/build-and-test-with-toolchain.sh')
                  ])
]

class IntegrationTask(MatrixTask):
    axes = OD([('valgrind', ['valgrind', False]),
               ('sanitizer', ['asan', 'tsan', False]),
               ('coverage', ['coverage', False]),
               ('version', ['latest', '5.0',
                            '4.4', '4.2', '4.0',
                            '3.6', '3.4', '3.2', '3.0']),
               ('topology', ['server', 'replica_set', 'sharded_cluster']),
               ('auth', [True, False]),
               ('sasl', ['sasl', 'sspi', False]),
               ('ssl', ['openssl', 'openssl-static', 'darwinssl', 'winssl', False]),
               ('cse', [True, False])])

    def __init__(self, *args, **kwargs):
        super(IntegrationTask, self).__init__(*args, **kwargs)
        if self.valgrind:
            self.add_tags('test-valgrind')
            self.add_tags(self.version)
            self.options['exec_timeout_secs'] = 14400
        elif self.coverage:
            self.add_tags('test-coverage')
            self.add_tags(self.version)
            self.options['exec_timeout_secs'] = 3600
        elif self.sanitizer == "asan":
            self.add_tags('test-asan', self.version)
            self.options['exec_timeout_secs'] = 3600
        elif self.sanitizer == "tsan":
            self.add_tags('tsan')
            self.add_tags(self.version)
        else:
            self.add_tags(self.topology,
                          self.version,
                          self.display('ssl'),
                          self.display('sasl'),
                          self.display('auth'))

        if self.cse:
            self.add_tags("client-side-encryption")
        # E.g., test-latest-server-auth-sasl-ssl needs debug-compile-sasl-ssl.
        # Coverage tasks use a build function instead of depending on a task.
        if self.valgrind:
            self.add_dependency('debug-compile-valgrind')
        elif self.sanitizer == "asan" and self.ssl and self.cse:
            self.add_dependency('debug-compile-asan-%s-cse' % (
                self.display('ssl'),))
        elif self.sanitizer == "asan" and self.ssl:
            self.add_dependency('debug-compile-asan-clang-%s' % (
                self.display('ssl'),))
        elif self.sanitizer == "asan":
            self.add_dependency('debug-compile-asan-clang')
        elif self.sanitizer == 'tsan' and self.ssl:
            self.add_dependency('debug-compile-tsan-%s' % self.display('ssl'))
        elif self.cse:
            self.add_dependency('debug-compile-%s-%s-cse' %
                                (self.display('sasl'), self.display('ssl')))
        elif not self.coverage:
            self.add_dependency('debug-compile-%s-%s' % (
                self.display('sasl'), self.display('ssl')))

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
            name_part(axis_name) for axis_name in self.axes
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
        commands.append(bootstrap(VERSION=self.version,
                                  TOPOLOGY=self.topology,
                                  AUTH='auth' if self.auth else 'noauth',
                                  SSL=self.display('ssl')))
        extra = {}
        if self.cse:
            extra["CLIENT_SIDE_ENCRYPTION"] = "on"
        commands.append(run_tests(VALGRIND=self.on_off('valgrind'),
                                  ASAN='on' if self.sanitizer == 'asan' else 'off',
                                  AUTH=self.display('auth'),
                                  SSL=self.display('ssl'),
                                  **extra))
        if self.coverage:
            commands.append(func('update codecov.io'))

        return task

    def _check_allowed(self):
        if self.sanitizer == 'tsan':
            require (self.ssl == 'openssl')
            prohibit (self.sasl)
            prohibit (self.valgrind)
            prohibit (self.coverage)
            prohibit (self.cse)
            prohibit (self.version == "3.0")

        if self.valgrind:
            prohibit(self.cse)
            prohibit(self.sanitizer)
            prohibit(self.sasl)
            require(self.ssl in ('openssl', False))
            prohibit(self.coverage)
            # Valgrind only with auth+SSL or no auth + no SSL.
            if self.auth:
                require(self.ssl == 'openssl')
            else:
                prohibit(self.ssl)

        if self.auth:
            require(self.ssl)

        if self.sasl == 'sspi':
            # Only one self.
            require(self.topology == 'server')
            require(self.version == 'latest')
            require(self.ssl == 'winssl')
            require(self.auth)

        if not self.ssl:
            prohibit(self.sasl)

        if self.coverage:
            prohibit(self.sasl)

            if self.auth:
                require(self.ssl == 'openssl')
            else:
                prohibit(self.ssl)

        if self.sanitizer == "asan":
            prohibit(self.sasl)
            prohibit(self.coverage)

            # Address sanitizer only with auth+SSL or no auth + no SSL.
            if self.auth:
                require(self.ssl == 'openssl')
            else:
                prohibit(self.ssl)

        if self.cse:
            require(self.version == 'latest' or parse_version(self.version) >= parse_version("4.2"))
            require(self.topology == 'server')
            if self.sanitizer != "asan":
                # limit to SASL=AUTO to reduce redundant tasks.
                require(self.sasl)
                require(self.sasl != 'sspi')
            prohibit(self.coverage)
            require(self.ssl)


all_tasks = chain(all_tasks, IntegrationTask.matrix())


class DNSTask(MatrixTask):
    axes = OD([('auth', [False, True]),
               ('ssl', ['openssl', 'winssl', 'darwinssl'])])

    name_prefix = 'test-dns'

    def __init__(self, *args, **kwargs):
        super(DNSTask, self).__init__(*args, **kwargs)
        sasl = 'sspi' if self.ssl == 'winssl' else 'sasl'
        self.add_dependency('debug-compile-%s-%s' %
                            (sasl, self.display('ssl')))

    @property
    def name(self):
        return self.name_prefix + '-' + '-'.join(
            self.display(axis_name) for axis_name in self.axes
            if getattr(self, axis_name))

    def to_dict(self):
        task = super(MatrixTask, self).to_dict()
        commands = task['commands']
        commands.append(
            func('fetch build', BUILD_NAME=self.depends_on['name']))

        orchestration = bootstrap(TOPOLOGY='replica_set',
                                  AUTH='auth' if self.auth else 'noauth',
                                  SSL='ssl')

        if self.auth:
            orchestration['vars']['AUTHSOURCE'] = 'thisDB'

        commands.append(orchestration)
        commands.append(run_tests(SSL='ssl',
                                  AUTH=self.display('auth'),
                                  DNS='dns-auth' if self.auth else 'on'))

        return task


all_tasks = chain(all_tasks, DNSTask.matrix())


class CompressionTask(MatrixTask):
    axes = OD([('compression', ['zlib', 'snappy', 'zstd', 'compression'])])
    name_prefix = 'test-latest-server'

    def __init__(self, *args, **kwargs):
        super(CompressionTask, self).__init__(*args, **kwargs)
        self.add_dependency('debug-compile-' + self._compressor_suffix())
        self.add_tags('compression', 'latest')
        self.add_tags(*self._compressor_list())

    @property
    def name(self):
        return self.name_prefix + '-' + self._compressor_suffix()

    def to_dict(self):
        task = super(CompressionTask, self).to_dict()
        commands = task['commands']
        commands.append(
            func('fetch build', BUILD_NAME=self.depends_on['name']))
        if self.compression == 'compression':
            orchestration_file = 'snappy-zlib-zstd'
        else:
            orchestration_file = self.compression

        commands.append(bootstrap(
            AUTH='noauth',
            SSL='nossl',
            ORCHESTRATION_FILE=orchestration_file))
        commands.append(run_tests(
            AUTH='noauth',
            SSL='nossl',
            COMPRESSORS=','.join(self._compressor_list())))

        return task

    def _compressor_suffix(self):
        if self.compression == 'zlib':
            return 'compression-zlib'
        elif self.compression == 'snappy':
            return 'compression-snappy'
        elif self.compression == 'zstd':
            return 'compression-zstd'
        else:
            return 'compression'

    def _compressor_list(self):
        if self.compression == 'zlib':
            return ['zlib']
        elif self.compression == 'snappy':
            return ['snappy']
        elif self.compression == 'zstd':
            return ['zstd']
        else:
            return ['snappy', 'zlib', 'zstd']


all_tasks = chain(all_tasks, CompressionTask.matrix())


class SpecialIntegrationTask(NamedTask):
    def __init__(self, task_name, depends_on='debug-compile-sasl-openssl',
                 suffix_commands=None, uri=None,
                 tags=None, version='latest', topology='server'):
        commands = [func('fetch build', BUILD_NAME=depends_on),
                    bootstrap(VERSION=version, TOPOLOGY=topology),
                    run_tests(uri)] + (suffix_commands or [])
        super(SpecialIntegrationTask, self).__init__(task_name,
                                                     commands=commands,
                                                     depends_on=depends_on,
                                                     tags=tags)


all_tasks = chain(all_tasks, [
    # Verify that retryWrites=true is ignored with standalone.
    SpecialIntegrationTask('retry-true-latest-server',
                           uri='mongodb://localhost/?retryWrites=true'),
    # Verify that retryWrites=true is ignored with old server.
    SpecialIntegrationTask('retry-true-3.4-replica-set',
                           version='3.4',
                           topology='replica_set'),
    SpecialIntegrationTask('test-latest-server-hardened',
                           'hardened-compile',
                           tags=['hardened', 'latest']),
])


class AuthTask(MatrixTask):
    axes = OD([('sasl', ['sasl', 'sspi', False]),
               ('ssl', ['openssl', 'openssl-static', 'darwinssl', 'winssl'])])

    name_prefix = 'authentication-tests'

    def __init__(self, *args, **kwargs):
        super(AuthTask, self).__init__(*args, **kwargs)
        self.add_tags('authentication-tests',
                      self.display('ssl'),
                      self.display('sasl'))

        self.add_dependency('debug-compile-%s-%s' % (
            self.display('sasl'), self.display('ssl')))

        self.commands.extend([
            func('fetch build', BUILD_NAME=self.depends_on['name']),
            func('run auth tests')])

    @property
    def name(self):
        rv = self.name_prefix + '-' + self.display('ssl')
        if self.sasl:
            return rv
        else:
            return rv + '-nosasl'

    def _check_allowed(self):
        both_or_neither(self.ssl == 'winssl', self.sasl == 'sspi')
        if not self.sasl:
            require(self.ssl == 'openssl')


all_tasks = chain(all_tasks, AuthTask.matrix())


class PostCompileTask(NamedTask):
    def __init__(self, *args, **kwargs):
        super(PostCompileTask, self).__init__(*args, **kwargs)
        self.commands.insert(
            0, func('fetch build', BUILD_NAME=self.depends_on['name']))


all_tasks = chain(all_tasks, [
    PostCompileTask(
        'test-valgrind-memcheck-mock-server',
        tags=['test-valgrind'],
        depends_on='debug-compile-valgrind',
        commands=[func('run mock server tests', VALGRIND='on', SSL='ssl')]),
    PostCompileTask(
        'test-asan-memcheck-mock-server',
        tags=['test-asan'],
        depends_on='debug-compile-asan-clang',
        commands=[func('run mock server tests', ASAN='on', SSL='ssl')]),
    PostCompileTask(
        'test-mongohouse',
        tags=[],
        depends_on='debug-compile-sasl-openssl',
        commands=[func('build mongohouse'),
                  func('run mongohouse'),
                  func('test mongohouse')]),
    # Compile with a function, not a task: gcov files depend on the absolute
    # path of the executable, so we can't compile as a separate task.
    NamedTask(
        'test-coverage-mock-server',
        tags=['test-coverage'],
        commands=[func('debug-compile-coverage-notest-nosasl-openssl'),
                  func('run mock server tests', SSL='ssl'),
                  func('update codecov.io')]),
    NamedTask(
        'test-coverage-latest-server-dns',
        tags=['test-coverage'],
        exec_timeout_secs=3600,
        commands=[func('debug-compile-coverage-notest-nosasl-openssl'),
                  bootstrap(TOPOLOGY='replica_set', AUTH='auth', SSL='ssl'),
                  run_tests(AUTH='auth', SSL='ssl', DNS='on'),
                  func('update codecov.io')]),
    NamedTask(
        'authentication-tests-memcheck',
        tags=['authentication-tests', 'valgrind'],
        exec_timeout_seconds=3600,
        commands=[
            shell_mongoc("""
                VALGRIND=ON DEBUG=ON CC='${CC}' MARCH='${MARCH}' SASL=AUTO \
                  SSL=OPENSSL CFLAGS='-DBSON_MEMCHECK' sh .evergreen/compile.sh
                """),
            func('run auth tests', valgrind='true')]),
    PostCompileTask(
        'test-versioned-api',
        tags=['versioned-api'],
        depends_on='debug-compile-nosasl-openssl',
        commands=[func('bootstrap mongo-orchestration', TOPOLOGY='server', AUTH='auth', SSL='ssl', VERSION='5.0', REQUIRE_API_VERSION='true'),
                  func('test versioned api', AUTH='auth', SSL='ssl')]),
    PostCompileTask(
        'test-versioned-api-accept-version-two',
        tags=['versioned-api'],
        depends_on='debug-compile-nosasl-nossl',
        commands=[func('bootstrap mongo-orchestration', TOPOLOGY='server', AUTH='noauth', SSL='nossl', VERSION='5.0', ORCHESTRATION_FILE='versioned-api-testing'),
                  func('test versioned api', AUTH='noauth', SSL='nossl')]),
])


class SSLTask(Task):
    def __init__(self, version, patch, cflags=None, fips=False, enable_ssl=False, **kwargs):
        full_version = version + patch + ('-fips' if fips else '')
        script = ''
        if cflags:
            script += 'export CFLAGS=%s\n' % (cflags,)

        script += "DEBUG=ON CC='${CC}' MARCH='${MARCH}' SASL=OFF"
        if enable_ssl:
            script += " SSL=" + enable_ssl
        elif 'libressl' in version:
            script += " SSL=LIBRESSL"
        else:
            script += " SSL=OPENSSL"

        if fips:
            script += " OPENSSL_FIPS=1"

        script += " sh .evergreen/compile.sh"

        super(SSLTask, self).__init__(commands=[
            func('install ssl', SSL=full_version),
            shell_mongoc(script),
            func('run auth tests', **kwargs),
            func('upload build')])

        self.version = version
        self.fips = fips
        self.enable_ssl = enable_ssl

    @property
    def name(self):
        s = 'build-and-run-authentication-tests-' + self.version
        if self.fips:
            return s + '-fips'
        if self.enable_ssl:
            return s + "-" + self.enable_ssl.lower()

        return s


all_tasks = chain(all_tasks, [
    SSLTask('openssl-0.9.8', 'zh', obsolete_tls=True),
    SSLTask('openssl-1.0.0', 't', obsolete_tls=True),
    SSLTask('openssl-1.0.1', 'u', cflags='-Wno-redundant-decls'),
    SSLTask('openssl-1.0.1', 'u', cflags='-Wno-redundant-decls', fips=True),
    SSLTask('openssl-1.0.2', 'l'),
    SSLTask('openssl-1.1.0', 'f'),
    SSLTask('libressl-2.5', '.2', require_tls12=True),
    SSLTask('libressl-3.0', '.2', require_tls12=True, enable_ssl="AUTO", cflags="-Wno-redundant-decls"),
    SSLTask('libressl-3.0', '.2', require_tls12=True),
])


class IPTask(MatrixTask):
    axes = OD([('client', ['ipv6', 'ipv4', 'localhost']),
               ('server', ['ipv6', 'ipv4'])])

    name_prefix = 'test-latest'

    def __init__(self, *args, **kwargs):
        super(IPTask, self).__init__(*args, **kwargs)
        self.add_tags('nossl', 'nosasl', 'server', 'ipv4-ipv6', 'latest')
        self.add_dependency('debug-compile-nosasl-nossl')
        self.commands.extend([
            func('fetch build', BUILD_NAME=self.depends_on['name']),
            bootstrap(IPV4_ONLY=self.on_off(server='ipv4')),
            run_tests(IPV4_ONLY=self.on_off(server='ipv4'),
                      URI={'ipv6': 'mongodb://[::1]/',
                           'ipv4': 'mongodb://127.0.0.1/',
                           'localhost': 'mongodb://localhost/'}[self.client])])

    def display(self, axis_name):
        return axis_name + '-' + getattr(self, axis_name)

    @property
    def name(self):
        return '-'.join([
            self.name_prefix, self.display('server'), self.display('client'),
            'noauth', 'nosasl', 'nossl'])

    def _check_allowed(self):
        # This would fail by design.
        if self.server == 'ipv4':
            prohibit(self.client == 'ipv6')

        # Default configuration is tested in other variants.
        if self.server == 'ipv6':
            prohibit(self.client == 'localhost')


all_tasks = chain(all_tasks, IPTask.matrix())

aws_compile_task = NamedTask('debug-compile-aws', commands=[shell_mongoc('''
        # Compile mongoc-ping. Disable unnecessary dependencies since mongoc-ping is copied to a remote Ubuntu 18.04 ECS cluster for testing, which may not have all dependent libraries.
        . .evergreen/find-cmake.sh
        export CC='${CC}'
        $CMAKE -DENABLE_SASL=OFF -DENABLE_SNAPPY=OFF -DENABLE_ZSTD=OFF -DENABLE_CLIENT_SIDE_ENCRYPTION=OFF .
        $CMAKE --build . --target mongoc-ping
'''), func('upload build')])

all_tasks = chain(all_tasks, [aws_compile_task])


class AWSTestTask(MatrixTask):
    axes = OD([('testcase', ['regular', 'ec2', 'ecs', 'lambda', 'assume_role']),
               ('version', ['latest', '5.0', '4.4'])])

    name_prefix = 'test-aws-openssl'

    def __init__(self, *args, **kwargs):
        super(AWSTestTask, self).__init__(*args, **kwargs)
        self.add_dependency('debug-compile-aws')
        self.commands.extend([
            func('fetch build', BUILD_NAME=self.depends_on['name']),
            bootstrap(AUTH="auth", ORCHESTRATION_FILE="auth-aws", VERSION=self.version, TOPOLOGY="server"),
            func('run aws tests', TESTCASE=self.testcase.upper())])

    @property
    def name(self):
        return '-'.join([self.name_prefix, self.testcase, self.version])


all_tasks = chain(all_tasks, AWSTestTask.matrix())


class OCSPTask(MatrixTask):
    axes = OD([('test', ['test_1', 'test_2', 'test_3', 'test_4', 'soft_fail_test', 'malicious_server_test_1',
                         'malicious_server_test_2', 'cache']),
               ('delegate', ['delegate', 'nodelegate']),
               ('cert', ['rsa', 'ecdsa']),
               ('ssl', ['openssl', 'openssl-1.0.1', 'darwinssl', 'winssl']),
               ('version', ['latest', '5.0', '4.4'])])

    name_prefix = 'test-ocsp'

    def __init__(self, *args, **kwargs):
        super(OCSPTask, self).__init__(*args, **kwargs)
        self.add_dependency('debug-compile-nosasl-%s' % (self.display('ssl')))
        self.add_tags('ocsp-' + self.display('ssl'))

    @property
    def name(self):
        return 'ocsp-' + self.display('ssl') + '-' + self.display('test') + '-' + self.display(
            'cert') + '-' + self.display('delegate') + '-' + self.display('version')

    def to_dict(self):
        task = super(MatrixTask, self).to_dict()
        commands = task['commands']
        commands.append(
            func('fetch build', BUILD_NAME=self.depends_on['name']))

        stapling = 'mustStaple'
        if self.test in ['test_3', 'test_4', 'soft_fail_test', 'cache']:
            stapling = 'disableStapling'
        if self.test in ['malicious_server_test_1', 'malicious_server_test_2']:
            stapling = 'mustStaple-disableStapling'

        orchestration_file = '%s-basic-tls-ocsp-%s' % (self.cert, stapling)
        orchestration = bootstrap(VERSION=self.version, TOPOLOGY='server', SSL='ssl', OCSP='on', ORCHESTRATION_FILE=orchestration_file)

        # The cache test expects a revoked response from an OCSP responder, exactly like TEST_4.
        test_column = 'TEST_4' if self.test == 'cache' else self.test.upper()

        commands.append(shell_mongoc(
            'TEST_COLUMN=%s CERT_TYPE=%s USE_DELEGATE=%s sh .evergreen/run-ocsp-responder.sh' % (
            test_column, self.cert, 'on' if self.delegate == 'delegate' else 'off')))
        commands.append(orchestration)
        if self.depends_on['name'] == 'debug-compile-nosasl-openssl-1.0.1':
            # LD_LIBRARY_PATH is needed so the in-tree OpenSSL 1.0.1 is found at runtime
            if self.test == 'cache':
                commands.append(shell_mongoc('export LD_LIBRARY_PATH=$(pwd)/install-dir/lib\n'
                    'CERT_TYPE=%s .evergreen/run-ocsp-cache-test.sh' % self.cert))
            else:
                commands.append(shell_mongoc('export LD_LIBRARY_PATH=$(pwd)/install-dir/lib\n'
                    'TEST_COLUMN=%s CERT_TYPE=%s sh .evergreen/run-ocsp-test.sh' % (self.test.upper(), self.cert)))
        else:
            if self.test == 'cache':
                commands.append(shell_mongoc('CERT_TYPE=%s .evergreen/run-ocsp-cache-test.sh' % self.cert))
            else:
                commands.append(shell_mongoc(
                    'TEST_COLUMN=%s CERT_TYPE=%s sh .evergreen/run-ocsp-test.sh' % (self.test.upper(), self.cert)))

        return task

    # Testing in OCSP has a lot of exceptions.
    def _check_allowed(self):
        if self.ssl == 'darwinssl':
            # Secure Transport quietly ignores a must-staple certificate with no stapled response.
            prohibit(self.test == 'malicious_server_test_2')

        # ECDSA certs can't be loaded (in the PEM format they're stored) on Windows/macOS. Skip them.
        if self.ssl == 'darwinssl' or self.ssl == 'winssl':
            prohibit(self.cert == 'ecdsa')

        # OCSP stapling is not supported on macOS or Windows.
        if self.ssl == 'darwinssl' or self.ssl == 'winssl':
            prohibit(self.test in ['test_1', 'test_2', 'cache'])

        if self.test == 'soft_fail_test' or self.test == 'malicious_server_test_2' or self.test == 'cache':
            prohibit(self.delegate == 'delegate')


all_tasks = chain(all_tasks, OCSPTask.matrix())

all_tasks = list(all_tasks)
