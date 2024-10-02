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

1. Change to the packaging branch, ``git checkout debian/unstable``, and make sure
   the working directorty is clean, ``git status``, and up-to-date, ``git pull``.
2. Because it is possible to have divergences between release branches, the next
   step depends on whether the release ``1.xx.y`` is a minor release (i.e.,
   ``y=0``) or a patch release (i.e., ``y>0``). In the case of a minor release
   perform step 3 and then continue with step 5, and in the case of a patch
   release, skip to step 4 and continue from there.
3. For a minor release, replace the upstream sources with those from the tagged
   release, ``git checkout --no-overlay 1.xx.0 . ':!debian'``. This operation
   should never produce a conflict. Commit the changes,
   ``git commit -m "Checkout upstream sources for '1.xx.0' into debian/unstable"``.
4. For a patch release, merge the release tag, ``git merge 1.xx.y``; in theory,
   as long as ``y>0`` this should not result in a conflict. If the merge stops
   with a conflict, then it will be necessary to investigate the problem.
5. Verify that there are no extraneous differences from the release tag,
   ``git diff 1.xx.y..HEAD --stat -- . ':!debian'``; the command should produce
   no output, and if any output is shown then that indicates differences in
   files outside the ``debian/`` directory.
6. If there were any files outside the ``debian/`` directory listed in the last
   step then replace them, ``git checkout 1.xx.y -- path/to/file1 path/to/file2``.
   Commit these changes,
   ``git commit -m "Fix-up, post Merge tag '1.xx.y' into debian/unstable"`` and
   repeat step 5.
7. Create a new changelog entry (use the command ``dch -i`` to ensure proper
   formatting), then adjust the version number on the top line of the changelog
   as appropriate.
8. Make any other necessary changes to the Debian packaging components (e.g.,
   update to standards version, dependencies, descriptions, etc.) and make
   relevant entries in ``debian/changelog`` as needed.
9. Use ``git add`` to stage the changed files for commit (only files in the
   ``debian/`` directory should be committed), then commit them (the ``debcommit``
   utility is helpful here).
10. Build the package with ``gbp buildpackage`` and inspect the resulting package
    files (at a minimum use ``debc`` on the ``.changes`` file in order to confirm
    files are installed to the proper locations by the proper packages and also
    use ``lintian`` on the ``.changes`` file in order to confirm that there are no
    unexpected errors or warnings; the ``lintian`` used for this check should
    always be the latest version as it is found in the unstable distribution).
11. If any changes are needed, make them, commit them, and rebuild the package.

    .. note:: It may be desirable to squash multiple commits down to a single commit before building the final packages.

12. Mark the package ready for release with the ``dch -r`` command, commit the
    resulting changes (after inspecting them),
    ``git commit debian/changelog -m 'mark ready for release'``.
13. Build the final packages. Once the final packages are built, they can be
    signed and uploaded and the version can be tagged using the ``--git-tag``
    option of ``gbp buildpackage``. The best approach is to build the packages,
    prepare everything and then upload. Once the archive has accepted the
    upload, then execute
    ``gbp buildpackage --git-tag --git-tag-only --git-sign-tags`` and push the
    commits on the ``debian/unstable`` branch as well as the new signed tag.
