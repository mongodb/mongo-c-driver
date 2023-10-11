###########################################
Building the C Driver Libraries from Source
###########################################

.. highlight:: shell-session
.. default-role:: bash

This page details how to download, unpack, configure, and build |libbson| and
|libmongoc| from their original source-code form.

.. ad-dropdown:: Extra information
  :admonition: hint
  :open:

  Dropdowns (like this one) contain extra information and explanatory details
  that are not required to complete the tutorial, but may be helpful for curious
  readers, and more advanced users that want an explanation of the meaning of
  certain tutorial steps.

The following page uses a few named "variables" that you must decide up-front.
When you see such a value referrenced in a tutorial step, you should substitute
the value into that step.

.. seealso::

  Before building, you may want to check that you are running on a supported
  platform. For the list of supported platforms, refer to the
  :doc:`/ref/platforms` page.


Choose a Version
****************

Before we begin, know what version of |mongo-c-driver| you will be downloading.
A list of available versions can be found on
`the GitHub repository tags page`__. (The current version written for this
documentation is |version.pre|.)

__ https://github.com/mongodb/mongo-c-driver/tags

For the remainder of this page, `$VERSION` will refer to the version number of
|mongo-c-driver| that you will be building for this tutorial.


.. _get-src:

Obtaining the Source
********************

There are two primary recommended methods of obtaining the |mongo-c-driver|
source code:

1. Clone the repository using `git` (recommended).
   :ref:`(See below) <learn.obtaining.git>`

2. Download a source archive at a specific version.
   :ref:`(See below) <learn.obtaining.archive>`

.. important::

  It is **highly recommended** that new users use a stable released version of
  the driver, rather than building from a development branch. When you
  `git clone` or download an archive of the repository, be sure to specify a
  release tag (e.g. with Git's `--branch` argument).

.. _learn.obtaining.git:

Downloading Using Git
=====================

Using Git, the C driver repository can be cloned from the GitHub URL
https://github.com/mongodb/mongo-c-driver.git. Git tags for released versions
are named after the version for which they correspond (e.g. "|version.pre|"). To
clone the repository using the command line, the following command may be used::

  $ git clone https://github.com/mongodb/mongo-c-driver.git --branch="$VERSION" "$SOURCE"

.. tip::

  Despite the name, `git-clone`\ 's `--branch` argument may also be used to
  clone from repository *tags*.

.. _learn.obtaining.archive:

Downloading a Release Archive
=============================

An archived snapshot of the repository can be obtained from the
`GitHub Releases Page`__. The ``mongo-c-driver-x.y.z.tar.gz`` archive attached
to any release contains the minimal set of files that you'll need for the build.

__ https://github.com/mongodb/mongo-c-driver/releases

.. tab-set::

  .. tab-item:: Using ``wget`` + ``tar``

    ::

      ## Download using wget:
      $ wget "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.tar.gz" \
          --output-document="mongo-c-driver-$VERSION.tar.gz"
      ## Extract using tar:
      $ tar xf "mongo-c-driver-$VERSION.tar.gz"

  .. tab-item:: Using ``curl`` + ``tar``

    ::

      ## Using curl:
      $ curl "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.tar.gz" \
          --output="mongo-c-driver-$VERSION.tar.gz"
      ## Extract using tar:
      $ tar xf "mongo-c-driver-$VERSION.tar.gz"

  .. tab-item:: PowerShell

    .. code-block:: pwsh-session

      ## Use Invoke-WebRequest:
      PS> $url = "https://github.com/mongodb/mongo-c-driver/archive/refs/tags/$VERSION.zip"
      PS> $file = "mongo-c-driver-$VERSION.zip"
      PS> Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $file
      ## Extract using Expand-Archive:
      PS> Expand-Archive mongo-c-driver-$VERSION.zip

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
  latest stable CMake available for your platform.

.. ad-dropdown:: Getting the Latest CMake
  :admonition: hint

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
  for 3.27.0 will then become available.

  The `--help` option can be passed to the shell script for more information.

For the remainder of this page, it will be assumed that `cmake` is available as
a command on your `PATH` environment variable and can be executed as "`cmake`"
from a shell. You can test this by requesting the `--version` from CMake from
the command line::

  $ cmake --version
  cmake version 3.21.4

  CMake suite maintained and supported by Kitware (kitware.com/cmake).

.. note::

  If you intend to build |libbson| *only*, then CMake is sufficient for the
  build. Additional C driver features may require additional external
  dependencies be installed, but we will not worry about them here.

.. XXX: Additional note for installing dependencies

  The Linux dependencies of sufficient version are likely available using the
  system package manager. For example, on **Debian**/**Ubuntu** based systems,
  they can be installed using APT::

    # apt install libssl-dev libsasl2-dev

  On **RedHat** based systems (**Fedora**, **CentOS**, **RockyLinux**,
  **AlmaLinux**, etc.)::

    # dnf install openssl-devel cyrus-sasl-devel

  Package names may vary between distributions.


.. _learn.get.configure:

Configuring for |libbson|
*************************

.. important::

  If you are building with Xcode [#xcode_env]_ or Visual Studio [#vs_env]_, you
  may need to execute CMake from within a special environment in which the
  respective toolchain is available.

Let the name `$BUILD` be the path `$SOURCE/_build`. This will be the directory
where our built files will be written by CMake.

With the source directory for |mongo-c-driver| at `$SOURCE` and build directory
`$BUILD`, the following command can be executed from a command-line to configure
the project with both |libbson| and |libmongoc|::

  $ cmake -S $SOURCE -B $BUILD \
    -D ENABLE_EXTRA_ALIGNMENT=OFF \
    -D ENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D BUILD_VERSION="$VERSION" \
    -D ENABLE_MONGOC=OFF


If all dependencies are satisfied, the above command should succeed and end
with::

  $ cmake …
  ## … (Lines of output) …
  -- Generating done
  -- Build files have been written to: $BUILD

If configuration failed with an error, refer to the CMake output for error
messages and information. Ensure that configuration succeeds before proceeding.

.. ad-dropdown:: What do these CMake arguments mean?
  :admonition: hint

  The `BUILD_VERSION` sets the version number that will be included in the build
  results. This should be set to the same value as the version of the source
  driver that was downloaded in :ref:`get-src`.

  The `ENABLE_EXTRA_ALIGNMENT` and `ENABLE_AUTOMATIC_INIT_AND_CLEANUP` are part
  of |mongo-c-driver|, and correspond to deprecated features that are only
  enabled by default for ABI compatibility purposes. It is highly recommended to
  disable these features whenever possible.

  The `ENABLE_MONGOC=OFF` argument disabled building |libmongoc|. We'll build
  that in the next section.

  The |cmvar:CMAKE_BUILD_TYPE| setting tells CMake what variant of code will be
  generated. In the case of `RelWithDebInfo`, optimized binaries will be
  produced, but still include debug information. The |cmvar:CMAKE_BUILD_TYPE| has no
  effect on Multi-Config generators (i.e. Visual Studio), which instead rely on
  the `--config` option when building/installing.

.. _CMAKE_BUILD_TYPE: https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html


.. _learn.get.build:

Building the Project
********************

After successfully configuring the project, the build can be executed by using
CMake::

  $ cmake --build $BUILD --config RelWithDebInfo --parallel

If configured properly and all dependencies are satisfied, then the above
command should proceed to compile and link the configured components. If the
above command fails, then there is likely an error with your environment, or you
are using an unsupported/untested platform. Refer to the build tool output for
more information.

.. ad-dropdown:: The ``--config`` option
  :admonition: hint
  :animate: fade-in

  The :option:`--config <cmake--build.--config>` option is used to set the build
  configuration to use in the case of Multi-Config generators (i.e. Visual
  Studio). It has no effect on other generators, which instead use
  |cmvar:CMAKE_BUILD_TYPE|.

.. _learn.get.install:

Installing the Built Results
****************************

Let `$PREFIX` be the path `$SOURCE/_install`. We can use CMake to install the
built results::

  $ cmake --install "$BUILD" --prefix "$PREFIX" --config RelWithDebInfo

This command will install the |mongo-c-driver| build results into the `$PREFIX`
directory.

.. ad-dropdown:: The ``--config`` option
  :admonition: hint
  :animate: fade-in

  The :external:option:`--config <cmake--install.--config>` option is only used
  for Multi-Config generators (i.e. Visual Studio) and is otherwise ignored. The
  value given for `--config` must be the same as was given for
  :external:option:`--config <cmake--build.--config>` with `cmake --build`.

.. seealso::

   The above snippet simply installs |mongo-c-driver| in a subdirectory of the
   source directory itself, but this is not a normal workflow. Once you feel
   compfortable about configuring and building |mongo-c-driver|, the page
   :doc:`/howto/source-install` will do a deeper dive on from-source
   installation options.


Configuring with |libmongoc|
****************************

If you followed the above steps starting from :ref:`learn.get.configure`, your
final result with only contain |libbson| and not the full C database driver
library. Building of |libmongoc| is enabled/disabled using the `ENABLE_MONGOC`
CMake variable. Re-run CMake again, but set `ENABLE_MONGOC` to `TRUE`::

  $ cmake -D ENABLE_MONGOC=ON $BUILD

If the above command succeeds, then the project has been reconfigured to build
with |libmongoc|. Follow the process at :ref:`learn.get.build` and
:ref:`learn.get.install` again to build and install |libmongoc|.

.. TODO:

  Add a guide on installing with the additional option features, which requires
  additional packages be available for the build.


.. rubric:: Footnotes

.. [#xcode_env]

  If you wish to configure and build the project with Xcode, the Xcode
  command-line tools need to be installed and made available in the environment.
  From within a command-line environment, run::

    $ xcode-select --install

  This will ensure that the compilers and linkers are available on your `$PATH`.

.. [#vs_env]

  If you wish to configure and build the project using Microsoft Visual C++,
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
