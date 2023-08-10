#################################################
How to: Install |libbson|/|libmongoc| from Source
#################################################

.. |pkg-config| replace:: :bolded-name:`pkg-config`
.. _pkg-config: https://www.freedesktop.org/wiki/Software/pkg-config/
.. _XDG base directory: https://wiki.archlinux.org/title/XDG_Base_Directory
.. _systemd file-hierarchy: https://man.archlinux.org/man/file-hierarchy.7

.. highlight:: console
.. default-role:: bash

.. important::

  This page assumes that you can successfully configure and build the components
  that you wish to install, which is detailed and explained on the
  :doc:`/learn/get/from-source` tutorial page. Whereas that tutorial walks
  through getting the sources built and a minimal install working, this page
  will offer deeper guidance on the nuance and available options for installing
  the |mongo-c-driver| libraries from such a from-source build.

|mongo-c-driver| uses CMake to generate its installation rules, and installs a
variety of artifacts of interest. For integration with downstream programs, the
:external+cmake:ref:`config file packages` and |pkg-config|_ files would be of
particular interest.

If you are intending to import |libbson| or |libmongoc| via CMake or pkg-config,
it can be helpful to be aware of how the respective tool searches for package
metadata.

.. tab-set::

  .. tab-item:: CMake Package Lookup

    CMake builds a set of search paths based on a set of prefixes, which are read
    from both the environment and from configure-time CMake settings.

    In particular, the `$PATH` environment variable will be used to construct
    the standard prefixes for the system. For each directory :math:`D` in
    `$PATH`:

    1. If the final path component of :math:`D` is "``bin``" or "``sbin``",
       :math:`D` is replaced with the parent path of :math:`D`.
    2. :math:`D` is added as a search prefix.

    This has the effect that common Unix-specific directories on `$PATH`, such
    as ``/usr/bin`` and ``/usr/local/bin`` will end up causing CMake to search
    in ``/usr`` and ``/usr/local`` is prefixes, respectively. If you have the
    directory `$HOME/.local/bin` on your `$PATH`, then the `$HOME/.local`
    directory will also be added to the search path. Having `$HOME/.local/bin`
    on `$PATH` is an increasingly common pattern for many Unix shells, and is
    recommended if you intend to do use a per-user-prefix_ for your installion.

    Additionally, the :any:`CMAKE_PREFIX_PATH <envvar:CMAKE_PREFIX_PATH>`
    *environment variable* will be used to construct a list of paths. By
    default, this environment variable is not defined.

    **On Windows**, the directories :batch:`%ProgramW6432%`,
    :batch:`%ProgramFiles%`, :batch:`%ProgramFiles(x86)%`,
    :batch:`%SystemDrive%\\Program Files`, and
    :batch:`%SystemDrive%\\Program Files (x86)` will also be added. (These come
    from the :any:`CMAKE_SYSTEM_PREFIX_PATH <variable:CMAKE_SYSTEM_PREFIX_PATH>`
    CMake variable, which is defined during CMake's platform detection.)

    .. seealso::

      For detailed information on package lookup, refer to CMake's
      :external+cmake:ref:`search procedure` section for full details.

  .. tab-item:: pkg-config Package Lookup

    The |pkg-config| command-line tool looks for ``.pc`` files in various
    directories, by default relative to the path of the |pkg-config| tool
    itself. To get the list of directories that your |pkg-config| will search by
    default, use the following command:

    .. code-block::
      :caption: Ask |pkg-config| what directories it will search by default

      $ pkg-config "pkg-config" --variable="pc_path"

    Additional directories can be specified using the `$PKG_CONFIG_PATH`
    environment variable. Such paths will be searched *before* the default
    |pkg-config| paths.

    **On Windows**, registry keys ``HKCU\Software\pkgconfig\PKG_CONFIG_PATH``
    and ``HKLM\Software\pkgconfig\PKG_CONFIG_PATH`` can be used to specify
    additional search directories for |pkg-config|. Adding directories to the
    ``HKCU\…`` key is recommended for persisting user-specific search
    directories.

    .. seealso::

       If you have `man` and |pkg-config| installed on your system, lookup
       procedures are detailed in `man 1 pkg-config`. This documentation may
       also be found at many man page archives on the web, such as
       `pkg-config at linux.die.net`__.

       __ https://linux.die.net/man/1/pkg-config


.. _howto.source-install.choosing-a-prefix:

Choosing a Prefix
*****************

We will call the directory for the user-local installation `$PREFIX`. Selecting
the path to this directory is somewhat arbitrary, but there are some
recommendations to consider. The `$PREFIX` directory is the path that you will
give to CMake or |pkg-config| when configuring a downstream project.


.. _per-user-prefix:

Using an Unprivileged User-Local Install Prefix (Recommended)
=============================================================

It is recommended that you install custom-built |mongo-c-driver| libraries in an
unprivileged filesystem location particular to the user account.

.. tab-set::

  .. tab-item:: macOS

    Unlike other Unix-like systems, macOS does not have a specific directory for
    user-local package installations, and it is up to the individual to create
    such directories themselves.

    The choice of directory to use is essentially arbitrary. For per-user
    installations, the only requirement is that the directory be writeable by
    the user that wishes to perform and use the installation.

    For the purposes of uniformity with other Unix variants, this guide will
    lightly recommend using `$HOME/.local` as a user-local installation prefix.
    This is based on the behavior specified by the `XDG base directory`_
    specifications and the `systemd file-hierarchy`_ common on Linux and various
    BSDs, but it is not a standard on other platforms.

  .. tab-item:: Linux & Other Unixes

    On Linux and BSD systems, it is common to use the `$HOME/.local` directory
    as the prefix for user-specific package installations. This convention
    originates in the `XDG base directory`_ specification and the
    `systemd file-hierarchy`_

    Because of its wide-spread use and support in many other tools, this guide
    recommends using `$HOME/.local` as a user-local installation prefix.

  .. tab-item:: Windows

    On Windows, there exists a dedicated directory for user-local files in
    :batch:`%UserProfile%\\AppData\\Local`. To reference it, expand the
    :batch:`%LocalAppData%` environment variable. (**Do not** use the
    :batch:`%AppData%` environment variable!)

    Despite this directory existing, it has no prescribed structure that suits
    our purposes. The choice of user-local installation prefix is arbitrary.
    This guide *strongly discourages* creating additional files and directories
    directly within the user's home directory.

    Consider using :batch:`%LocalAppData%\\MongoDB` as a prefix for the purposes
    of manually installed components.


.. _source-install.system-prefix:

Selecting a System-Wide Installation Prefix
===========================================

If you wish to install the |mongo-c-driver| libraries in a directory that is
visible to all users, there are a few standard options.

.. tab-set::

  .. tab-item:: Linux, macOS, BSD, or Other Unix

    Using an install `$PREFIX` of ``/usr/local/`` is the primary recommendation
    for all Unix platforms, but this may vary on some obscure systems.

    .. warning::

      **DO NOT** use ``/usr/`` nor ``/`` (the root directory) as a prefix: These
      directories are designed to be carefully managed by the system. The
      ``/usr/local`` directory is intentionally reserved for the purpose of
      unmanaged software installation.

    Alternatively, consider installing to a distinct directory that can be
    easily removed or relocated, such as ``/opt/mongo-c-driver/``. This will be
    easily identifiable and not interact with other software on the system
    without explicitly opting-in.

  .. tab-item:: Windows

    .. warning::

       It is **strongly discouraged** to manually install software system-wide
       on Windows. Prefer instead to
       :ref:`use a per-user unprivileged installation prefix <per-user-prefix>`.

    If you wish to perform a system-wide installation on Windows, prefer to use
    a named subdirectory of :batch:`%ProgramData%`, which does not require
    administrative privileges to read and write. (e.g.
    :batch:`%ProgramData%\\mongo-c-driver`)


Installing with CMake
*********************

After you have successfully configured and built the libraries and have selected
a suitable `$PREFIX`, you can install the built results. Let the name `$BUILD`
refer to the directory where you executed the build (this is the directory that
contains ``CMakeCache.txt``, among many other files).

From a command line, the installation into your chosen `$PREFIX` can be run via
CMake using the
:external:option:`cmake --install subcommand <cmake.--install>`::

  $ cmake --install "$BUILD" --prefix "$PREFIX"

.. important::

  If you configured the libraries while using a *multi-config generator* (e.g
  Visual Studio, Xcode), then you will also need to pass the
  :external:option:`--config <cmake--install.--config>` command-line option, and
  must pass the value for the build configuration that you wish to install. For
  any chosen value of `--config` used for installation, you must also have
  previously executed a :external:option:`cmake --build <cmake.--build>` within
  that directory with that same `--config` value.

.. note::

  If you chose to use a system-wide installation `$PREFIX`, it is possible that
  you will need to execute the installation as a privileged user. If you *cannot
  run* or *do not want to run* the installation as a privileged user, you should
  instead `use a per-user installation prefix <per-user-prefix_>`_.

.. hint::

  It is not necessary to set a |cmvar:CMAKE_INSTALL_PREFIX| if you use the
  :external:option:`--prefix <cmake--install.--prefix>` command-line option with
  `cmake --install`. The `--prefix` option will override whatever was specified
  by |cmvar:CMAKE_INSTALL_PREFIX| when the project was configured.
