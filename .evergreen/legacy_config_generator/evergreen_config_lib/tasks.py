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

from evergreen_config_generator.functions import (func, s3_put)
from evergreen_config_generator.tasks import (
    both_or_neither, MatrixTask, NamedTask, prohibit, require, Task)
from evergreen_config_lib import shell_mongoc
from pkg_resources import parse_version


class CompileTask(NamedTask):
    def __init__(self, task_name, tags=None, config='debug',
                 compression='default', continue_on_err=False,
                 suffix_commands=None, depends_on=None,
                 extra_script=None, prefix_commands=None, sanitize=(),
                 **kwargs):
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

        # Environment variables for .evergreen/scripts/compile.sh.
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

        if sanitize:
            self.compile_sh_opt['SANITIZE'] = ','.join(sanitize)

        self.continue_on_err = continue_on_err

    def to_dict(self):
        task = super(CompileTask, self).to_dict()

        task['commands'].extend(self.prefix_commands)

        script = 'env'
        for opt, value in sorted(self.compile_sh_opt.items()):
            script += ' %s="%s"' % (opt, value)

        script += ' bash .evergreen/scripts/compile.sh'
        script += self.extra_script
        task['commands'].append(shell_mongoc(
            script, add_expansions_to_env=True))
        task['commands'].append(func('upload-build'))
        task['commands'].extend(self.suffix_commands)
        return task


class SpecialTask(CompileTask):
    def __init__(self, *args, **kwargs):
        super(SpecialTask, self).__init__(*args, **kwargs)
        self.add_tags('special')


class CompileWithClientSideEncryption(CompileTask):
    def __init__(self, *args, **kwargs):
        # Compiling with ClientSideEncryption support requires linking against the library libmongocrypt.
        super(CompileWithClientSideEncryption, self).__init__(*args,
                                                              COMPILE_LIBMONGOCRYPT="ON",
                                                              EXTRA_CONFIGURE_FLAGS="-DENABLE_PIC=ON -DENABLE_CLIENT_SIDE_ENCRYPTION=ON",
                                                              **kwargs)
        self.add_tags('client-side-encryption', 'special')


class CompileWithClientSideEncryptionAsan(CompileTask):
    def __init__(self, *args, **kwargs):
        super(CompileWithClientSideEncryptionAsan, self).__init__(*args,
                                                                  CFLAGS="-fno-omit-frame-pointer",
                                                                  COMPILE_LIBMONGOCRYPT="ON",
                                                                  CHECK_LOG="ON",
                                                                  sanitize=[
                                                                      'address'],
                                                                  EXTRA_CONFIGURE_FLAGS="-DENABLE_CLIENT_SIDE_ENCRYPTION=ON -DENABLE_EXTRA_ALIGNMENT=OFF",
                                                                  PATH='/usr/lib/llvm-3.8/bin:$PATH',
                                                                  **kwargs)
        self.add_tags('client-side-encryption')


class LinkTask(NamedTask):
    def __init__(self, task_name, suffix_commands, orchestration=True, **kwargs):
        if orchestration == 'ssl':
            # Actual value of SSL does not matter here so long as it is not 'nossl'.
            bootstrap_commands = [
                func('fetch-det'),
                func('bootstrap-mongo-orchestration', SSL="openssl")
            ]
        elif orchestration:
            bootstrap_commands = [
                func('fetch-det'),
                func('bootstrap-mongo-orchestration')
            ]
        else:
            bootstrap_commands = []

        super(LinkTask, self).__init__(
            task_name=task_name,
            depends_on=OD([('name', 'make-release-archive'),
                           ('variant', 'releng')]),
            commands=bootstrap_commands + suffix_commands,
            **kwargs)


all_tasks = [
    CompileTask('hardened-compile',
                tags=['hardened'],
                compression=None,
                CFLAGS='-fno-strict-overflow -D_FORTIFY_SOURCE=2 -fstack-protector-all -fPIE -O',
                LDFLAGS='-pie -Wl,-z,relro -Wl,-z,now'),
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
    CompileTask('debug-compile-no-counters',
                tags=['debug-compile', 'no-counters'],
                ENABLE_SHM_COUNTERS='OFF'),
    SpecialTask('debug-compile-asan-clang',
                tags=['debug-compile', 'asan-clang'],
                compression='zlib',
                CFLAGS='-fno-omit-frame-pointer',
                CHECK_LOG='ON',
                sanitize=['address'],
                EXTRA_CONFIGURE_FLAGS='-DENABLE_EXTRA_ALIGNMENT=OFF'),
    SpecialTask('debug-compile-asan-clang-openssl',
                tags=['debug-compile', 'asan-clang'],
                compression='zlib',
                CFLAGS='-fno-omit-frame-pointer',
                CHECK_LOG='ON',
                sanitize=['address'],
                EXTRA_CONFIGURE_FLAGS="-DENABLE_EXTRA_ALIGNMENT=OFF",
                SSL='OPENSSL'),
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
                      ENABLE_SSL="AUTO")]),
    LinkTask('link-with-cmake-snappy',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      ENABLE_SNAPPY="ON")]),
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
                      ENABLE_SSL="AUTO")]),
    LinkTask('link-with-cmake-snappy-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1,
                      ENABLE_SNAPPY="ON")]),
    LinkTask('link-with-cmake-mac-deprecated',
             suffix_commands=[
                 func('link sample program',
                      BUILD_SAMPLE_WITH_CMAKE=1,
                      BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=1)]),
    LinkTask('link-with-cmake-windows',
             suffix_commands=[func('link sample program MSVC')]),
    LinkTask('link-with-cmake-windows-ssl',
             suffix_commands=[
                 func('link sample program MSVC', ENABLE_SSL="AUTO")],
             orchestration='ssl'),
    LinkTask('link-with-cmake-windows-snappy',
             suffix_commands=[
                 func('link sample program MSVC', ENABLE_SNAPPY="ON")]),
    LinkTask('link-with-cmake-mingw',
             suffix_commands=[func('link sample program mingw')]),
    LinkTask('link-with-pkg-config',
             suffix_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-mac',
             suffix_commands=[func('link sample program')]),
    LinkTask('link-with-pkg-config-ssl',
             suffix_commands=[func('link sample program', ENABLE_SSL="AUTO")]),
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
                               'sh .evergreen/scripts/debian_package_build.sh'),
                  s3_put(local_file='deb.tar.gz',
                         remote_file='${branch_name}/mongo-c-driver-debian-packages-${CURRENT_VERSION}.tar.gz',
                         content_type='${content_type|application/x-gzip}'),
                  s3_put(local_file='deb.tar.gz',
                         remote_file='${branch_name}/${revision}/${version_id}/${build_id}/${execution}/mongo-c-driver-debian-packages.tar.gz',
                         content_type='${content_type|application/x-gzip}')]),
    NamedTask('rpm-package-build',
              commands=[
                  shell_mongoc('sh .evergreen/scripts/build_snapshot_rpm.sh'),
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
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\scripts\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\scripts\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check-msvc',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_mongoc(r'''
                  export CC="Visual Studio 14 2015 Win64"
                  BSON_ONLY=1 cmd.exe /c .\\.evergreen\\scripts\\install-uninstall-check-windows.cmd
                  cmd.exe /c .\\.evergreen\\scripts\\install-uninstall-check-windows.cmd''')]),
    NamedTask('install-uninstall-check',
              depends_on=OD([('name', 'make-release-archive'),
                             ('variant', 'releng')]),
              commands=[shell_mongoc(r'''
                  DESTDIR="$(pwd)/dest" sh ./.evergreen/scripts/install-uninstall-check.sh
                  BSON_ONLY=1 sh ./.evergreen/scripts/install-uninstall-check.sh
                  sh ./.evergreen/scripts/install-uninstall-check.sh''')]),
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
                  shell_mongoc(
                      'bash ./.evergreen/scripts/build-and-test-with-toolchain.sh')
              ])
]


class CoverageTask(MatrixTask):
    axes = OD([('version', ['latest']),
               ('topology', ['replica_set']),
               ('auth', [True]),
               ('sasl', ['sasl']),
               ('ssl', ['openssl']),
               ('cse', [True])])

    def __init__(self, *args, **kwargs):
        super(CoverageTask, self).__init__(*args, **kwargs)

        self.name_prefix = 'test-coverage'

        self.add_tags('test-coverage')
        self.add_tags(self.version)

        if self.cse:
            self.add_tags("client-side-encryption")

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
        task = super(CoverageTask, self).to_dict()
        commands = task['commands']
        if self.depends_on:
            commands.append(
                func('fetch-build', BUILD_NAME=self.depends_on['name']))

        # Limit coverage tests to test-coverage-latest-replica-set-auth-sasl-openssl-cse.
        commands.append(
            func('compile coverage', SASL='AUTO', SSL='OPENSSL'))

        commands.append(func('fetch-det'))
        commands.append(func('bootstrap-mongo-orchestration',
                             MONGODB_VERSION=self.version,
                             TOPOLOGY=self.topology,
                             AUTH='auth' if self.auth else 'noauth',
                             SSL=self.display('ssl')))
        extra = {}

        if self.cse:
            extra["CLIENT_SIDE_ENCRYPTION"] = "on"
            commands.append(func('fetch-det'))
            commands.append(func('run-mock-kms-servers'))
        extra["COVERAGE"] = 'ON'
        commands.append(func('run-tests',
                             AUTH=self.display('auth'),
                             SSL=self.display('ssl'),
                             **extra))
        commands.append(func('upload coverage'))
        commands.append(func('update codecov.io'))

        return task

    def _check_allowed(self):
        # Limit coverage tests to test-coverage-latest-replica-set-auth-sasl-openssl-cse.
        require(self.topology == 'replica_set')
        require(self.auth)
        require(self.sasl == 'sasl')
        require(self.ssl == 'openssl')
        require(self.cse)
        require(self.version == 'latest')

        # Address sanitizer only with auth+SSL or no auth + no SSL.
        if self.auth:
            require(self.ssl == 'openssl')
        else:
            prohibit(self.ssl)

        if self.cse:
            require(self.version == 'latest' or parse_version(
                self.version) >= parse_version("4.2"))
            if self.version == 'latest' or parse_version(self.version) >= parse_version("6.0"):
                # FLE 2.0 Client-Side Encryption tasks on 6.0 require a non-standalone topology.
                require(self.topology in ('server', 'replica_set'))
            else:
                require(self.topology == 'server')
            # limit to SASL=AUTO to reduce redundant tasks.
            require(self.sasl)
            require(self.sasl != 'sspi')
            require(self.ssl)


all_tasks = chain(all_tasks, CoverageTask.matrix())


class DNSTask(MatrixTask):
    axes = OD([('auth', [False, True]),
               ('loadbalanced', [False, True]),
               ('ssl', ['openssl', 'winssl', 'darwinssl'])
               ])

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
            func('fetch-build', BUILD_NAME=self.depends_on['name']))
        commands.append(func('fetch-det'))

        if self.loadbalanced:
            orchestration = func('bootstrap-mongo-orchestration',
                                 TOPOLOGY='sharded_cluster',
                                 AUTH='auth' if self.auth else 'noauth',
                                 SSL='ssl',
                                 LOAD_BALANCER='on')
        else:
            orchestration = func('bootstrap-mongo-orchestration',
                                 TOPOLOGY='replica_set',
                                 AUTH='auth' if self.auth else 'noauth',
                                 SSL='ssl')

        if self.auth:
            orchestration['vars']['AUTHSOURCE'] = 'thisDB'

        commands.append(orchestration)

        dns = 'on'
        if self.loadbalanced:
            dns = 'loadbalanced'
            commands.append(func("fetch-det"))
            commands.append(func(
                "start load balancer", MONGODB_URI="mongodb://localhost:27017,localhost:27018"))
        elif self.auth:
            dns = 'dns-auth'
        commands.append(func('run-tests',
                             SSL='ssl',
                             AUTH=self.display('auth'),
                             DNS=dns))

        return task

    def _check_allowed(self):
        prohibit(self.loadbalanced and self.auth)
        # Load balancer tests only run on some Linux hosts in Evergreen until CDRIVER-4041 is resolved.
        prohibit(self.loadbalanced and self.ssl in ["darwinssl", "winssl"])


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
            func('fetch-build', BUILD_NAME=self.depends_on['name']))
        commands.append(func('fetch-det'))

        if self.compression == 'compression':
            orchestration_file = 'snappy-zlib-zstd'
        else:
            orchestration_file = self.compression

        commands.append(func('bootstrap-mongo-orchestration',
                             AUTH='noauth',
                             SSL='nossl',
                             ORCHESTRATION_FILE=orchestration_file))
        commands.append(func('run-tests',
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
        commands = [func('fetch-build', BUILD_NAME=depends_on),
                    func('fetch-det'),
                    func('bootstrap-mongo-orchestration',
                         MONGODB_VERSION=version,
                         TOPOLOGY=topology),
                    func('run-tests', URI=uri)] + (suffix_commands or [])
        super(SpecialIntegrationTask, self).__init__(task_name,
                                                     commands=commands,
                                                     depends_on=depends_on,
                                                     tags=tags)


all_tasks = chain(all_tasks, [
    # Verify that retryWrites=true is ignored with standalone.
    SpecialIntegrationTask('retry-true-latest-server',
                           uri='mongodb://localhost/?retryWrites=true'),
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
            func('fetch-build', BUILD_NAME=self.depends_on['name']),
            func('prepare-kerberos'),
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
            0, func('fetch-build', BUILD_NAME=self.depends_on['name']))


all_tasks = chain(all_tasks, [
    PostCompileTask(
        'test-asan-memcheck-mock-server',
        tags=['test-asan'],
        depends_on='debug-compile-asan-clang',
        commands=[func('run mock server tests', ASAN='on', SSL='ssl')]),
    PostCompileTask(
        'test-mongohouse',
        tags=[],
        depends_on='debug-compile-sasl-openssl',
        commands=[func('fetch-det'),
                  func('build mongohouse'),
                  func('run mongohouse'),
                  func('test mongohouse')]),
    NamedTask(
        'authentication-tests-asan-memcheck',
        tags=['authentication-tests', 'asan'],
        commands=[
            shell_mongoc("""
            env SANITIZE=address DEBUG=ON SASL=AUTO SSL=OPENSSL EXTRA_CONFIGURE_FLAGS='-DENABLE_EXTRA_ALIGNMENT=OFF' bash .evergreen/scripts/compile.sh
            """, add_expansions_to_env=True),
            func('prepare-kerberos'),
            func('run auth tests', ASAN='on')]),
    PostCompileTask(
        'test-versioned-api',
        tags=['versioned-api'],
        depends_on='debug-compile-nosasl-openssl',
        commands=[func('fetch-det'),
                  func('bootstrap-mongo-orchestration', TOPOLOGY='server', AUTH='auth',
                       SSL='ssl', MONGODB_VERSION='5.0', REQUIRE_API_VERSION='true'),
                  func('run-tests', MONGODB_API_VERSION=1, AUTH='auth', SSL='ssl')]),
    PostCompileTask(
        'test-versioned-api-accept-version-two',
        tags=['versioned-api'],
        depends_on='debug-compile-nosasl-nossl',
        commands=[func('fetch-det'),
                  func('bootstrap-mongo-orchestration', TOPOLOGY='server', AUTH='noauth',
                       SSL='nossl', MONGODB_VERSION='5.0', ORCHESTRATION_FILE='versioned-api-testing'),
                  func('run-tests', MONGODB_API_VERSION=1, AUTH='noauth', SSL='nossl')]),
])


class SSLTask(Task):
    def __init__(self, version, patch, cflags=None, fips=False, enable_ssl=False, **kwargs):
        full_version = version + patch + ('-fips' if fips else '')
        script = 'env'
        if cflags:
            script += f' CFLAGS={cflags}'

        script += ' DEBUG=ON SASL=OFF'

        if enable_ssl:
            script += " SSL=" + enable_ssl
        elif 'libressl' in version:
            script += " SSL=LIBRESSL"
        else:
            script += " SSL=OPENSSL"

        script += " bash .evergreen/scripts/compile.sh"

        super(SSLTask, self).__init__(commands=[
            func('install ssl', SSL=full_version),
            shell_mongoc(script, add_expansions_to_env=True),
            func('run auth tests', **kwargs),
            func('upload-build')])

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
    SSLTask('openssl-1.0.1', 'u', cflags='-Wno-redundant-decls', ),
    SSLTask('openssl-1.0.1', 'u', cflags='-Wno-redundant-decls', fips=True),
    SSLTask('openssl-1.0.2', 'l', cflags='-Wno-redundant-decls', ),
    SSLTask('openssl-1.1.0', 'l'),
    SSLTask('libressl-2.5', '.2', require_tls12=True),
    SSLTask('libressl-3.0', '.2', require_tls12=True, enable_ssl="AUTO"),
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
            func('fetch-build', BUILD_NAME=self.depends_on['name']),
            func("fetch-det"),
            func('bootstrap-mongo-orchestration',
                 IPV4_ONLY=self.on_off(server='ipv4')),
            func('run-tests',
                 IPV4_ONLY=self.on_off(server='ipv4'),
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
        . .evergreen/scripts/find-cmake.sh
        export CC='${CC}'
        $CMAKE -DENABLE_SASL=OFF -DENABLE_SNAPPY=OFF -DENABLE_ZSTD=OFF -DENABLE_CLIENT_SIDE_ENCRYPTION=OFF .
        $CMAKE --build . --target mongoc-ping
'''), func('upload-build')])

all_tasks = chain(all_tasks, [aws_compile_task])


class AWSTestTask(MatrixTask):
    axes = OD([('testcase', ['regular', 'ec2', 'ecs', 'lambda', 'assume_role']),
               ('version', ['latest', '5.0', '4.4'])])

    name_prefix = 'test-aws-openssl'

    def __init__(self, *args, **kwargs):
        super(AWSTestTask, self).__init__(*args, **kwargs)
        self.add_dependency('debug-compile-aws')
        self.commands.extend([
            func('fetch-build', BUILD_NAME=self.depends_on['name']),
            func('fetch-det'),
            func('bootstrap-mongo-orchestration',
                 AUTH="auth",
                 ORCHESTRATION_FILE="auth-aws",
                 MONGODB_VERSION=self.version,
                 TOPOLOGY="server"),
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

        # OCSP tests should run with a batchtime of 14 days. Avoid running OCSP
        # tests in patch builds by default (only in commit builds).
        task['patchable'] = False

        commands = task['commands']
        commands.append(
            func('fetch-build', BUILD_NAME=self.depends_on['name']))
        commands.append(func('fetch-det'))

        stapling = 'mustStaple'
        if self.test in ['test_3', 'test_4', 'soft_fail_test', 'cache']:
            stapling = 'disableStapling'
        if self.test in ['malicious_server_test_1', 'malicious_server_test_2']:
            stapling = 'mustStaple-disableStapling'

        orchestration_file = '%s-basic-tls-ocsp-%s' % (self.cert, stapling)
        orchestration = func('bootstrap-mongo-orchestration',
                             MONGODB_VERSION=self.version,
                             TOPOLOGY='server',
                             SSL='ssl',
                             OCSP='on',
                             ORCHESTRATION_FILE=orchestration_file)

        # The cache test expects a revoked response from an OCSP responder, exactly like TEST_4.
        test_column = 'TEST_4' if self.test == 'cache' else self.test.upper()
        use_delegate = 'ON' if self.delegate == 'delegate' else 'OFF'

        commands.append(shell_mongoc(f'''
        TEST_COLUMN={test_column} CERT_TYPE={self.cert} USE_DELEGATE={use_delegate} bash .evergreen/scripts/run-ocsp-responder.sh
        '''))

        commands.append(orchestration)

        if self.depends_on['name'] == 'debug-compile-nosasl-openssl-1.0.1':
            # LD_LIBRARY_PATH is needed so the in-tree OpenSSL 1.0.1 is found at runtime
            if self.test == 'cache':
                commands.append(shell_mongoc(f'''
                LD_LIBRARY_PATH=$(pwd)/install-dir/lib CERT_TYPE={self.cert} bash .evergreen/scripts/run-ocsp-cache-test.sh
                '''))
            else:
                commands.append(shell_mongoc(f'''
                LD_LIBRARY_PATH=$(pwd)/install-dir/lib TEST_COLUMN={self.test.upper()} CERT_TYPE={self.cert} bash .evergreen/scripts/run-ocsp-test.sh
                '''))
        else:
            if self.test == 'cache':
                commands.append(shell_mongoc(f'''
                CERT_TYPE={self.cert} bash .evergreen/scripts/run-ocsp-cache-test.sh
                '''))
            else:
                commands.append(shell_mongoc(f'''
                TEST_COLUMN={self.test.upper()} CERT_TYPE={self.cert} bash .evergreen/scripts/run-ocsp-test.sh
                '''))

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


class LoadBalancedTask(MatrixTask):
    axes = OD([
        ('asan', [True]),
        # The SSL library the C driver is built with.
        ('build_ssl', ['openssl']),
        # Whether tests are run with SSL connections.
        ('test_ssl', [True, False]),
        ('test_auth', [True, False]),
        ('version', ['5.0', 'latest'])
    ])

    def _check_allowed(self):
        # Test with both SSL and auth, or neither.
        prohibit(self.test_ssl != self.test_auth)

    def __init__(self, *args, **kwargs):
        super(LoadBalancedTask, self).__init__(*args, **kwargs)
        if self.asan and self.build_ssl == "openssl":
            self.add_dependency('debug-compile-asan-clang-openssl')
            self.add_tags('test-asan')
        else:
            raise RuntimeError(
                "unimplemented configuration for LoadBalancedTask")

        self.add_tags(self.version)

    # Return the task name.
    # Example: test-loadbalanced-asan-auth-openssl-latest
    @property
    def name(self):
        name = "test-loadbalanced"
        if self.asan:
            name += "-asan"
        if self.test_auth:
            name += "-auth"
        else:
            name += "-noauth"
        if self.test_ssl:
            name += "-" + self.build_ssl
        else:
            name += "-nossl"
        if self.version:
            name += "-" + self.version
        return name

    def to_dict(self):
        task = super(MatrixTask, self).to_dict()
        commands = task['commands']
        commands.append(
            func('fetch-build', BUILD_NAME=self.depends_on['name']))
        commands.append(func("fetch-det"))

        orchestration = func('bootstrap-mongo-orchestration',
                             TOPOLOGY='sharded_cluster',
                             AUTH='auth' if self.test_auth else 'noauth',
                             SSL='ssl' if self.test_ssl else 'nossl',
                             MONGODB_VERSION=self.version,
                             LOAD_BALANCER='on')
        commands.append(orchestration)
        commands.append(func("start load balancer",
                             MONGODB_URI="mongodb://localhost:27017,localhost:27018"))
        commands.append(func('run-tests',
                             ASAN='on' if self.asan else 'off',
                             SSL='ssl' if self.test_ssl else 'nossl',
                             AUTH='auth' if self.test_auth else 'noauth',
                             LOADBALANCED='loadbalanced'))

        return task


all_tasks = chain(all_tasks, LoadBalancedTask.matrix())

all_tasks = list(all_tasks)
