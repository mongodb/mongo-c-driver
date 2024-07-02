Debian Packaging
################

.. highlight:: console
.. default-role:: bash

Release Publishing
******************

.. ! NOTE: Updates to these instructions should be synchronized to the corresponding
   ! C++ release process documentation located in the "etc/releasing.md" file in the C++
   ! driver repository

.. note::

    The Debian package release is made only after the upstream release has been
    tagged.

    After a C driver release is completed (i.e. the :ref:`releasing.jira` step
    of the release process), a new Jira ticket will be automatically created to
    track the work of the corresponding release of the Debian package for the C
    driver (`example ticket <https://jira.mongodb.org/browse/CDRIVER-5554>`__).

To publish a new release Debian package, perform the following:

1. For the first Debian package release on a **new release branch**, edit
   ``debian/gbp.conf`` and update the ``upstream-branch`` and ``debian-branch``
   variables to match the name of the new release branch (e.g., ``r1.xx``); both
   variables should have the same value.

2. Create a new changelog entry (use the command `dch -i` to ensure proper
   formatting), then adjust the version number on the top line of the changelog
   as appropriate.
3. Make any other necessary changes to the Debian packaging components (e.g.,
   update to standards version, dependencies, descriptions, etc.) and make
   relevant entries in ``debian/changelog`` as needed.
4. Use `git add` to stage the changed files for commit (only files in the
   `debian/` directory should be committed), then commit them (the `debcommit`
   utility is helpful here).
5. Build the package with `gbp buildpackage` and inspect the resulting package
   files (at a minimum use `debc` on the `.changes` file in order to confirm
   files are installed to the proper locations by the proper packages and also
   use `lintian` on the `.changes` file in order to confirm that there are no
   unexpected errors or warnings; the `lintian` used for this check should
   always be the latest version as it is found in the unstable distribution)
6. If any changes are needed, make them, commit them, and rebuild the package.

   .. note:: It may be desirable to squash multiple commits down to a single commit before building the final packages.

7. Once the final packages are built, they can be signed and uploaded and the
   version can be tagged using the `--git-tag` option of `gbp buildpackage`.
8. After the commit has been tagged, switch from the release branch to the
   master branch and cherry-pick the commit(s) made on the release branch that
   touch only the Debian packaging (this will ensure that the packaging and
   especially the changelog on the master remain up to date).
9. The final steps are to sign and upload the package, push the commits on the
   release branch and the master branch to the remote, and push the Debian
   package tag.
