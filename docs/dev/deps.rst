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