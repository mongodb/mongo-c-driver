Earthly
#######

Earthly_ is a CI and development tool that containerizes aspects of the CI
pipeline so that they run consistently across hosts and across time.

.. highlight:: console

.. _earthly: https://earthly.dev
.. _earthly secrets: https://docs.earthly.dev/docs/guides/secrets
.. _docker: https://www.docker.com/
.. _podman: https://podman.io/

Running Earthly
***************

.. note::

   Before you can run Earthly, you will need either Podman_ or Docker_ installed
   on your system. If you have trouble getting Earthly to work with Podman,
   refer to `the Earthly Podman Guide`__.

   __ https://docs.earthly.dev/docs/guides/podman

While it is possible to download and install Earthly_ on your system, this task
itself is automated by scripting within the ``mongo-c-driver`` repository. To
run Earthly from the ``mongo-c-driver`` repository, use `tools/earthly.sh`.

.. script:: tools/earthly.sh

   This script will download and cache an Earthly executable on-the-fly and execute
   it with the same command-line arguments that were passed to the script. For any
   ``earthly`` command, you may run `tools/earthly.sh` in its place.

   .. code-block:: console
      :caption: Example Earthly output

      $ ./tools/earthly.sh --version
      earthly-linux-amd64 version v0.8.3 70916968c9b1cbc764c4a4d4d137eb9921e97a1f linux/amd64; EndeavourOS

   Running Earthly via this script ensures that the same Earthly version is used
   across all developer and CI systems.

   .. envvar:: EARTHLY_VERSION

      The `tools/earthly.sh` script inspects the `EARTHLY_VERSION` environment
      variable and downloads+executes that version of Earthly. This allows one
      to test new Earthly versions without modifying the `tools/earthly.sh`
      script.

      This environment variable has a default value, so specifying it is not
      required. Updating the default value in `tools/earthly.sh` will change the
      default version of Earthly that is used by the script.


Testing Earthly
===============

To verify that Earthly is running, execute the `+version-current` Earthly
target. This will exercise most Earthly functionality without requiring any
special parameters or modifying the working directory::

   $ ./tools/earthly.sh +version-current
   Init 🚀
   ————————————————————————————————————————————————————————————————————————————————

   [... snip ...]

   ========================== 🌍 Earthly Build  ✅ SUCCESS ==========================


Earthly Targets
***************

This section documents some of the available Earthly targets contained in the
top-level ``Earthfile`` in the repository. More are defined, and can be
enumerated using ``earthly ls`` or ``earthly doc`` in the root of the repository.

.. program:: +signed-release
.. earthly-target:: +signed-release

   Creates signed release artifacts using `+release-archive` and `+sign-file`.

   .. seealso:: `releasing.gen-archive`, which uses this target.

   .. earthly-artifact:: +signed-release/dist/

      A directory artifact that contains the `+release-archive/release.tar.gz`,
      `+release-archive/ssdlc_compliance_report.md`, and
      `+sign-file/signature.asc` for the release. The exported filenames are
      based on the `--version` argument.

   .. rubric:: Parameters
   .. option:: --sbom_branch <branch>

      Forwarded to `+release-archive --sbom_branch`

   .. option:: --version <version>

      Affects the output filename and archive prefix paths in
      `+signed-release/dist/` and sets the default value for `--ref`.

   .. option:: --ref <git-ref>

      Specify the git revision to be archived. Forwarded to
      `+release-archive --ref`. If unspecified, archives the Git tag
      corresponding to `--version`.

   .. rubric:: Secrets

   Secrets for the `+sbom-download`, `+snyk-test`, and `+sign-file` targets are
   required for this target.


.. program:: +release-archive
.. earthly-target:: +release-archive

   Generate a source release archive of the repository at a specifiy branch.
   Requires the secrets for `+sbom-download` and `+snyk-test`.

   .. earthly-artifact:: +release-archive/release.tar.gz

      The resulting source distribution archive for the specified branch. The
      generated archive includes the source tree, but also includes other
      release artifacts that are generated on-the-fly when invoked e.g. the
      `+sbom-download/augmented-sbom.json` artifact.

   .. earthly-artifact:: +release-archive/ssdlc_compliance_report.md

      The SSDLC compliance report for the release. This file is based on the
      content of ``etc/ssdlc.md``, which has certain substrings replaced based
      on attributes of the release.

   .. rubric:: Parameters
   .. option:: --sbom_branch <branch>

      Forwarded as `+sbom-download --branch` to download the augmented SBOM.

   .. option:: --ref <git-ref>

      Specifies the Git revision that is used when we use ``git archive`` to
      generate the repository archive snapshot. Use of ``git archive`` ensures
      that the correct contents are included in the archive (i.e. it won't
      include local changes and ignored files). This also allows a release
      snapshot to be taken for a non-active branch.

   .. option:: --prefix <path>

      Specify a filepath prefix to appear in the generated filepaths. This has
      no effect on the files archived, which is selected by
      `+release-archive --ref`.


.. program:: +sbom-download
.. earthly-target:: +sbom-download

   Download an `augmented SBOM <augmented-sbom>` from Silk for a given project
   branch. This target explicitly disables caching, because the upstream SBOM
   file can change arbitrarily.

   .. earthly-artifact:: +sbom-download/augmented-sbom.json

      The `augmented SBOM <augmented-sbom>` downloaded from Silk for the requested branch.

   .. rubric:: Parameters
   .. option:: --branch <branch>

      **Required**. Specifies the branch of the repository from which we are
      requesting an SBOM.

      .. note::

         It is *required* that the `Silk asset group <silk-asset-group>` has
         been created for the given branch before the `+sbom-download` target
         can succeed. See: `+create-silk-asset-group`

   .. rubric:: Secrets
   .. envvar::
      SILK_CLIENT_ID
      SILK_CLIENT_SECRET

      **Required**. [#creds]_

      .. seealso:: `earthly.secrets`

.. program:: +sign-file
.. earthly-target:: +sign-file

   Signs a file using Garasign. Use of this target requires authenticating
   against the MongoDB Artifactory installation! (Refer to:
   `earthly.artifactory-auth`)

   .. earthly-artifact:: +sign-file/signature.asc

      The detached PGP signature for the input file.

   .. rubric:: Parameters
   .. option:: --file <filepath>

      **Required**. Specify a path to a file (on the host) to be signed. This
      file must be a descendant of the directory that contains the ``Earthfile``
      and must not be excluded by an ``.earthlyignore`` file (it is copied
      into the container using the COPY__ command.)

      __ https://docs.earthly.dev/docs/earthfile#copy

   .. rubric:: Secrets
   .. envvar::
      GRS_CONFIG_USER1_PASSWORD
      GRS_CONFIG_USER1_USERNAME

      **Required**. [#creds]_

      .. seealso:: `earthly.secrets`

   .. _earthly.artifactory-auth:

   Authenticating with Artifactory
   ===============================

   In order to run `+sign-file` or any target that depends upon it, the
   container engine client\ [#oci]_ will need to be authenticated with the
   MongoDB Artifactory instance.

   Authenticating can be done using the container engine's command-line
   interface. For example, with Podman::

      $ podman login "artifactory.corp.mongodb.com"

   Which will prompt you for a username and password if you are not already
   authenticated with the host.\ [#creds]_ If you are already authenticated, this
   command will have no effect.

.. earthly-target:: +version-current

   Generates a ``VERSION_CURRENT`` file for the current repository.

   .. earthly-artifact:: +version-current/VERSION_CURRENT

      A plaintext file containing the current version number.

.. earthly-target:: +sbom-generate

   Updates the `etc/cyclonedx.sbom.json` file **in-place** based on the contents
   of `etc/purls.txt` and the existing `etc/cyclonedx.sbom.json`.

   After running this target, the contents of the `etc/cyclonedx.sbom.json` file
   may change.

   .. seealso:: `sbom-lite` and `sbom-lite-updating`


.. program:: +create-silk-asset-group
.. earthly-target:: +create-silk-asset-group

   Creates a new `Silk asset group <silk-asset-group>` for a branch in the
   repository. This target executes the `tools/create-silk-asset-group.py`
   script with the appropriate arguments.

   .. note:: For branches that execute in CI, running this target manually is
      not necessary, as it is run automatically for every build.

   .. rubric:: Parameters
   .. option:: --branch <branch>

      The repository branch for which to create the new asset group. If not
      specified, the branch name will be inferred by asking Git.

   .. rubric:: Secrets
   .. envvar::
         SILK_CLIENT_ID
         SILK_CLIENT_SECRET
      :noindex:

      **Required**. [#creds]_

      .. seealso:: `earthly.secrets`

.. program:: +snyk-monitor-snapshot
.. earthly-target:: +snyk-monitor-snapshot

   Executes `snyk monitor`__ on a crafted snapshot of the remote repository.
   This target specifically avoids an issue outlined in `snyk scanning` (See
   "Caveats"). Clones the repository at the given `--branch` for the snapshot
   being taken.

   __ https://docs.snyk.io/snyk-cli/commands/monitor

   .. seealso:: Release process step: `releasing.snyk`

   .. rubric:: Parameters
   .. option:: --branch <branch>

      **Required**. The name of the branch or tag to be snapshot.

   .. option:: --name <name>

      **Required**. The name for the monitored snapshot ("target reference") to
      be stored in the Snyk server.

      .. note:: If a target with this name already exists in the Snyk server,
         then executing `+snyk-monitor-snapshot` will replace that target.

   .. option:: --remote <url | "local">

      The repository to be snapshot and posted to Snyk for monitoring. Defaults
      to the upstream repository URL. Use ``"local"`` to snapshot the repository
      in the working directory (not recommended except for testing).

   .. rubric:: Secrets
   .. envvar:: SNYK_ORGANIZATION

      The API ID of the Snyk_ organization that owns the Snyk target. For the C
      driver, this secret must be set to the value for the organization ID of
      the MongoDB **dev-prod** Snyk organization.

      **Do not** use the organization ID of **mongodb-default**.

      The `SNYK_ORGANIZATION` may be obtained from the `Snyk organization page
      <https://app.snyk.io/org/dev-prod/manage/settings>`_.

      .. _snyk: https://app.snyk.io

   .. envvar:: SNYK_TOKEN

      Set this to the value of an API token for accessing Snyk in the given
      `SNYK_ORGANIZATION`.

      The `SNYK_TOKEN` may be obtained from the `Snyk account page <https://app.snyk.io/account>`_.

.. program:: +snyk-test
.. earthly-target:: +snyk-test

   Execute `snyk test`__ on the local copy. This target specifically avoids an
   issue outlined in `Snyk Scanning > Caveats <snyk caveats>`.

   __ https://docs.snyk.io/snyk-cli/commands/test

   .. earthly-artifact:: +snyk-test/snyk.json

      The Snyk JSON data result of the scan.

   .. rubric:: Secrets
   .. envvar:: SNYK_TOKEN
      :noindex:

      See: `SNYK_TOKEN`


.. _earthly.secrets:

Setting Earthly Secrets
***********************

Some of the above targets require defining `earthly secrets`_\
[#creds]_.

To pass secrets to Earthly, it is easiest to use a ``.secret`` file in the root
of the repository. Earthly will implicitly read this file for secrets required
during execution. Your ``.secret`` file will look something like this:

.. code-block:: ini
   :caption: Example ``.secret`` file content

   GRS_CONFIG_USER1_USERNAME=john.doe
   GRS_CONFIG_USER1_PASSWORD=hunter2

.. warning::

   Earthly supports passing secrets on the command line, **but this is not
   recommended** as the secrets will then be stored in shell history.

   Shell history can be supressed by prefixing a command with an extra space,
   but this is more cumbersome than using environment variables or a ``.secret``
   file.

.. seealso:: `The Earthly documentation on passing secrets <earthly secrets_>`_

.. [#oci]

   You container engine client will probably be Docker or Podman. Wherever the
   :bash:`podman` command is used, :bash:`docker` should also work equivalently.


.. [#creds]

   Credentials are expected to be available in `AWS Secrets Manager
   <https://wiki.corp.mongodb.com/display/DRIVERS/Using+AWS+Secrets+Manager+to+Store+Testing+Secrets>`_ under
   ``drivers/c-driver``.
