External Dependencies
#####################

The C driver libraries make use of several external dependencies that are
tracked separately from the library itself. These can be classified as:

.. glossary::

   Bundled Dependency
   Vendored Dependency

      A dependency is "bundled" or "vendored" if its source lives directly
      within the repository that is consuming it. We control the exact version
      of the dependency that is used during the build process, aiding in
      reproducibility and consumability for users.

   System Dependency

      A "system" dependency is any dependency that we rely on being provided by
      the user when they are building or executing the software. We have no
      control over the versions of system dependencies that the user might
      provide to us.


The Software Bill of Materials (SBOM)
*************************************

The repository and driver releases contain a machine-readable
:abbr:`SBOM (software bill of materials)` that describes the contents of the
:term:`vendored dependencies <vendored dependency>` used in the distributed
driver libraries.

The SBOM comes in two flavors: The SBOM-\ *lite* and the *augmented* SBOM
(aSBOM). The SBOM-lite is the stored in `etc/cyclonedx.sbom.json` within the
repository, and is mostly generated from `etc/purls.txt`.


.. _sbom-lite:

The SBOM-Lite
=============

The SBOM-lite is "lite" in that it contains only the minimum information
required to later build the `augmented SBOM`_. It contains the name, version,
copyright statements, URLs, and license identifiers of the dependencies.

.. file:: etc/cyclonedx.sbom.json

   The `SBOM-lite`_ for the C driver project. This is injested automatically and
   asynchronously by Silk to produce the `augmented SBOM`_. This file is
   generated semi-automatically from `etc/purls.txt` and the `+sbom-generate`
   Earthly target.

   .. warning:: This file is **partially generated**! Prefer to edit `etc/purls.txt`!
      Refer to: `sbom-lite-updating`

.. file:: etc/purls.txt

   This file contains a set of purls__ (package URLs) that refer to the
   third-party components that are :term:`vendored <vendored dependency>` into
   the repository. A purl is a URL string that identifies details about software
   packages, including their upstream location and version.

   This file is maintained by hand and should be updated whenever any vendored
   dependencies are updated within the repository. Refer to: `sbom-lite-updating`

   __ https://github.com/package-url/purl-spec


.. _sbom-lite-updating:

Updating the SBOM-Lite
----------------------

Whenever a change is made to the set of vendored dependencies in the repository,
the content of `etc/purls.txt` should be updated and the SBOM-lite
`etc/cyclonedx.sbom.json` file re-generated. The contents of the SBOM lite JSON
*should not* need to be updated manually. Refer to the following process:

1. Add/remove/update the package URLs in `etc/purls.txt` corresponding to the
   vendored dependencies that are being changed.
2. Execute the `+sbom-generate` Earthly target successfully.
3. Stage and commit the changes to *both* `etc/purls.txt` and
   `etc/cyclonedx.sbom.json` simultaneously.

.. _augmented-SBOM:
.. _augmented SBOM:

The Augmented SBOM
==================

At time of writing, the *augmented SBOM* file is not stored within the
repository [#f1]_, but is instead obtained on-the-fly as part of the release
process, as this is primarily a release artifact for end users.

The augmented SBOM contains extra data about the dependencies from the
`SBOM-lite <sbom-lite>`, including vulnerabilities known at the time of the
augmented SBOM's generation.

The augmented SBOM is produced automatically and asynchronously as part of an
external process that is not contained within the repository itself. The
augmented SBOM is downloaded from an internal service using the `+sbom-download`
Earthly target, which is automatically included in the release archive for the
`+release-archive` target.

.. [#f1]

   This may change in the future depending on how the process may evolve.


.. _silk-asset-group:

Silk Asset Groups
*****************

.. note:: A Silk asset group will be created automatically for each branch that
   is executed in CI.

We use Silk's *asset groups* to allow tracking of multiple versions of the
SBOM-lite_ simultaneously (i.e. one for each release branch). These asset groups
correspond to branches within the repository, and are created automatically when
CI executes for the first time on a particular branch. If you need an asset
group for a branch that has not run in CI, use the `+create-silk-asset-group`
Earthly target to create the asset group on-demand.

Note that Silk pulls from the upstream Git repository for an asset group, so
creating an asset group for a branch that does not exist in the main upstream
repository will not work.

.. file:: tools/create-silk-asset-group.py

   A Python script that will create an `asset group <silk-asset-group>` in Silk
   based on a set of parameters. Execute with ``--help`` for more information.
   For the C driver, it is easier to use the `+create-silk-asset-group` Earthly
   target.


.. _snyk scanning:

Snyk Scanning
*************

Snyk_ is a tool that detects dependencies and tracks vulnerabilities in
packages. Snyk is used in a limited fashion to detect vulnerabilities in the
bundled dependencies in the C driver repository.

.. rubric:: Caveats

At the time of writing (June 20, 2024), Snyk has trouble scanning the C driver
repository for dependencies. If given the raw repository, it will detect the
mongo-c-driver package as the sole "dependency" of itself, and it fails to
detect the other dependencies within the project. The `+snyk-test` Earthly
target is written to avoid this issue and allow Snyk to accurately detect other
dependencies within the project.

Due to difficultry coordinating the behavior of Snyk and Silk at time of
writing, vulnerability collection is partially a manual process. This is
especially viable as the native code contains a very small number of
dependencies and it is trivial to validate the output of Snyk by hand.

.. seealso:: The `releasing.snyk` step of the release process

.. _snyk: https://app.snyk.io

.. program:: tools/snyk-vulns.py
.. file:: tools/snyk-vulns.py

   A Python script that generates the third-party vulnerability report that is
   included in the release archive. This script is used by the `+vuln-report-md`
   Earthly target.

   The script reads Snyk_\ -generated JSON data from standard input and writes
   the resulting Markdown document to standard output.

   .. rubric:: Parameters
   .. option:: --cve-exclude CVE-YYYY-NNNNNNNN[,...]

      A comma-separated list of CVE identifiers that will be excluded from the
      generated report even if they appear within the JSON data input. Each ID
      must be of the form ``CVE-YYYY-NNNNNNNN``. An empty string is treated as
      an empty list.

      .. seealso:: The `+vuln-report-md --cve_exclude` argument exposes this setting to
            Earthly.
