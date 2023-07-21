###########################################
Building the C Driver Libraries from Source
###########################################

.. highlight:: shell-session
.. default-role:: bash

To build the C driver from source, one first needs to obtain the source code.
The easiest way to obtain the source is to download it from
`the official GitHub repository`__.

__ https://github.com/mongodb/mongo-c-driver

.. note::

  In the following examples, `$SOURCE_DIR` refers to the directory in which you
  will be placing the |mongo-c-driver| source code, and `$VERSION` refers to the
  version of the C driver that you are. The current version written for this
  documentation is |version.pre|.

It is **highly recommended** that new users use a stable released version of the
driver, rather than building from a development branch.


Obtaining the Source
********************

There are two primary recommended methods of obtaining the |mongo-c-driver|
source code:

1. Clone the repository using `git`. :ref:`(See below) <learn.obtaining.git>`

   - This is useful if you wish to view the version history, test unreleased
     features/fixes, or contribute to the codebase.
   - This is straightforward and an equivalent process across all platforms.

2. Download a source archive at a specific version.
   :ref:`(See below) <learn.obtaining.archive>`

   - This is useful without needing to have `git` available and can be done
     using only built-in programs that ship with most major operating systems,
     but requires a slightly different process between platforms.
   - The download is smaller and stored in a single file.


.. _learn.obtaining.git:

Downloading Using Git
=====================

Using Git, the C driver repository can be cloned from the GitHub URL
``https://github.com/mongodb/mongo-c-driver.git``. Git tags for released
versions are named after the version for which they correspond (e.g.
"|version.pre|"). To clone the repository using the command line, the following
command may be used::

  $ git clone https://github.com/mongodb/mongo-c-driver.git --branch="$VERSION" "$SOURCE_DIR"


.. _learn.obtaining.archive:

Downloading a Release Archive
=============================

An archived snapshot of the repository can be obtained from the
`GitHub Releases Page`__. The ``mongo-c-driver-x.y.z.tar.gz`` archive attached
to any release contains a minimal version of the repository with only the files
necessary to build the driver binaries.

__ https://github.com/mongodb/mongo-c-driver/releases

The archive can be downloaded and extracted from a command line using one of
several tools, depending on what is available::

  ## Using wget:
  $ wget "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.tar.gz" --output-document="mongo-c-driver-$VERSION.tar.gz"
  ## Extract using tar:
  $ tar -x -f "mongo-c-driver-$VERSION.tar.gz"

::

  ## Using curl:
  $ curl "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.tar.gz" --output="mongo-c-driver-$VERSION.tar.gz"
  ## Extract using tar:
  $ tar -x -f "mongo-c-driver-$VERSION.tar.gz"

.. code-block:: pwsh

  ## Using PowerShell:
  Invoke-WebRequest `
      -UseBasicParsing `
      -Uri "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.zip" `
      -OutFile "mongo-c-driver-$VERSION.zip"
  ## Extract using Expand-Archive:
  Expand-Archive mongo-c-driver-$VERSION.zip

The above commands will create a new directory `mongo-c-driver-$VERSION` within
the directory in which you ran the `tar`/`Expand-Archive` command (**note**:
PowerShell will create an additional intermediate subdirectory of the same
name). This directory is the root of the driver source tree (which we refer to
as `$SOURCE` in these documents). The `$SOURCE` directory should contain the
top-level `CMakeLists.txt` file.


Obtaining Prerequisites
***********************

In order to build the project, a few prerequisites need to be available.

Both |libmongoc| and |libbson| projects use CMake__ for build configuration.

__ https://cmake.org

.. note::

  It is *highly recommended* -- but not *required* -- that you download the
  latest stable CMake available for your platform. [#cmake]_

For the remainder of this page, it will be assumed that `cmake` is available as
a command on your `PATH` environment variable and can be executed as "`cmake`"
from a shell. You can test this by requesting the `--version` from CMake from
the command line::

  $ cmake --version
  cmake version 3.21.4

  CMake suite maintained and supported by Kitware (kitware.com/cmake).

.. important::

  A CMake of version 3.15 *or newer* is **required** for building the source.

If you intend to build |libbson| *only*, then CMake is sufficient for the build.
To build the full C driver, additional packages are required on some platforms:

- On Linux, OpenSSL/LibreSSL development components are required.
- On Linux, Cyrus SASL development components are required.

.. note::

  Additional C driver features may require additional external dependencies be
  installed, but we will not worry about them here.

.. note::

  The Linux dependencies of sufficient version are likely available using the
  system package manager. For example, on **Debian**/**Ubuntu** based systems,
  they can be installed using APT::

    # apt install libssl-dev libsasl2-dev

  On **RedHat** based systems (**Fedora**, **CentOS**, **RockyLinux**,
  **AlmaLinux**, etc.)::

    # dnf install openssl-devel cyrus-sasl-devel

  Package names may vary between distributions.


.. _learn.obtaining.configuring:

Configuring the Project
***********************

.. important::

  If you are building with Xcode [#xcode_env]_ or Visual Studio [#vs_env]_, you
  may need to execute CMake from within a special environment in which the
  resepective toolchain is available.

Going forward, the name `$BUILD` refers to an ephemeral directory which will
contain the intermediate files for the build. It is *highly* recommended to use
a separate directory from `$SOURCE` for `$BUILD`. A reasonable default would be
`$SOURCE/_build`, which would place all build results in the `_build/`
subdirectory of `$SOURCE`.

With the source directory for |mongo-c-driver| at `$SOURCE` and build directory
`$BUILD`, the following command can be executed from a command-line to configure
the project with both |libbson| and |libmongoc|::

  $ cmake -S $SOURCE -B $BUILD \
    -D ENABLE_EXTRA_ALIGNMENT=FALSE \
    -D ENABLE_AUTOMATIC_INIT_AND_CLEANUP=FALSE \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo

.. note::

  To configure the project without |libmongoc| (and only configure |libbson|),
  pass the additional command-line arguments "`-D ENABLE_MONGOC=FALSE`" with the
  above command.

If all dependencies are satisfied, the above command should succeed and end
with::

  $ cmake …
  ## … (Lines of output) …
  -- Generating done
  -- Build files have been written to: $BUILD

If configuration failed with an error, refer to the CMake output for error
messages and information. Ensure that configuration succeeds before proceeding.

.. note::

  If you attempt to change the `-G` option, then CMake will fail to configure.
  Run with `--fresh`__ to clean out the configuration when using `-G` (`--fresh`
  is only available in CMake 3.24 or newer).

  __ https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-fresh

.. note::

  The `ENABLE_EXTRA_ALIGNMENT` and `ENABLE_AUTOMATIC_INIT_AND_CLEANUP`
  correspond to deprecated features that are only enabled for compatibility
  purposes. It is highly recommended to disable these features whenever
  possible.

  The |cmvar:CMAKE_BUILD_TYPE| setting tells CMake what variant of code will be
  generated. In the case of `RelWithDebInfo`, optimized binaries will be
  produced, but still include debug information. The |cmvar:CMAKE_BUILD_TYPE| has no
  effect on Multi-Config generators (i.e. Visual Studio), which instead rely on
  the `--config` option when building/installing.

.. _CMAKE_BUILD_TYPE: https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html

Building the Project
********************

After a successful :ref:`configuration <learn.obtaining.configuring>`, the
build can be executed using CMake to launch the underlying build tool::

  $ cmake --build $BUILD --config RelWithDebInfo --parallel

If configured properly and all dependencies are satisfied, then the above
command should proceed to compile and link the configured components. If the
above command fails, then there is likely an error with your environment, or you
are using an unsupported/untested platform. Refer to the build tool output for
more information.

.. note::

  The :option:`--config <cmake--build.--config>` option is used to set the build
  configuration to use in the case of Multi-Config generators (i.e. Visual
  Studio). It has no effect on other generators, which instead use
  |cmvar:CMAKE_BUILD_TYPE|.


Installing the Built Results
****************************

To use the built C driver in another project, it is required that the build
results are installed in a directory that is visible to the consuming projects.
The actual directory used for the install is not significant, but should be
known and specified explicitly. The recommended way to do this is to set the
|cmvar:CMAKE_INSTALL_PREFIX| variable.

Let `$PREFIX` be the absolute path to a directory that we will use for the
installation. For simplicity, we'll set this to `$SOURCE/_install`. Invoke
`cmake` again and set the |cmvar:CMAKE_INSTALL_PREFIX| option::

  $ cmake -D CMAKE_INSTALL_PREFIX=$PREFIX $BUILD
  ## … (Lines of output) …
  -- Generating done
  -- Build files have been written to: $BUILD

We can now install the built results::

  $ cmake --install $BUILD --config RelWithDebInfo

.. note::

  The :external:option:`--config <cmake--install.--config>` option is only used
  for Multi-Config generators (i.e. Visual Studio) and is otherwise ignored. The
  value given for `--config` must be the same as was given for
  :external:option:`--config <cmake--build.--config>` with `cmake --build`.


This will now install the |mongo-c-driver| build results into the directory
specified by |cmvar:CMAKE_INSTALL_PREFIX|, ready to be imported into downstream
projects.

.. important::

  Unless certain special values of |cmvar:CMAKE_INSTALL_PREFIX| were used,
  downstream projects will want to specify |cmvar:CMAKE_PREFIX_PATH| to include
  the value of `$PREFIX` when configuring. This will allow |cmcmd:find_package|
  to find |libmongoc| and |libbson|.

.. rubric:: Footnotes

.. [#cmake]

  A new stable release of CMake can be obtained from
  `the CMake downloads page`__.

  __ https://cmake.org/download/#latest

  For Windows and macOS, simply download the CMake `.msi`/`.dmg` (not the
  `.zip`/`.tar.gz`) and use it to install CMake.

  On Linux, download the self-extracting shell script (ending with `.sh`) and
  execute it using the `sh` utility, passing the appropriate arguments to
  perform the install. For example, with the CMake 3.27.0 on the `x86_64`
  platform, the following command can be used on the
  `cmake-3.27.0-linux-x86_64.sh` script::

    $ sh cmake-3.27.0-linux-x86_64.sh --prefix="$HOME/.local" --exclude-subdir --skip-license

  Assuming that `$HOME/.local/bin` is on your `$PATH` list, the `cmake` command
  for 3.27.0 will then become available. The `--help` option can be passed to
  the shell script for more information.

.. [#xcode_env]

  If you wish to configure and build the project with Xcode, the Xcode
  command-line tools need to be installed and made available in the environment.
  From within a command-line environment, run::

    $ xcode-select --install

  This will ensure that the compilers and linkers are available on your `$PATH`.

.. [#vs_env]

  If you with to configure and build the project using Microsoft Visual C++,
  then the Visual C++ tools and environment variables may need to be set when
  running any CMake or build command.

  In many cases, CMake will detect a Visual Studio installation and
  automatically load the environment itself when it is executed. This automatic
  detection can be controlled with CMake's :option:`-G <cmake.-G>`,
  :option:`-T <cmake.-T>`, and :option:`-A <cmake.-A>` options. The `-G` option
  is the most significant, as it selects which Visual Studio version will be
  used. The versions of Visual Studio supported depends on the version of CMake
  that you have installed.
  `A list of supported Visual Studio versions can be found here`__

  __ https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators

  For greater control and more tooling options, it is recommended to run
  commands from within a Visual Studio *Developer PowerShell* (preferred) or
  *Developer Command Prompt* (legacy).

  For more information, refer to:
  `Visual Studio Developer Command Prompt and Developer PowerShell`__ and
  `Use the Microsoft C++ toolset from the command line`__ on the Microsoft
  Visual Studio documentation pages.

  __ https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell
  __ https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line
