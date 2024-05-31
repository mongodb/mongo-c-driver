.. title:: Releasing the MongoDB C Driver
.. rubric:: Releasing the MongoDB C Driver
.. The use of "rubric" here is to give the page a title header that does
   not effect the section numbering, which we use to enumerate the steps of the
   process. This page is not included directly in a visible toctree, and is instead
   linked manually with a :doc: role. If this page is included in a visible toctree, then
   the top-level sections would be inlined into the toctree in an unintuitive manner.

This page documents the process required for releasing a new version of the
MongoDB C driver library. The release includes the following steps:

.. sectnum::
.. contents:: Release Process

.. _latest-build: https://spruce.mongodb.com/commits/mongo-c-driver
.. _evg-release: https://spruce.mongodb.com/commits/mongo-c-driver-latest-release
.. _evg-release-settings: https://spruce.mongodb.com/project/mongo-c-driver-latest-release/settings/general


Check that Tests Are Passing
############################

Before releasing, ensure that the latest commits on the branch are successful in
CI.

- For minor releases, `refer to the tests in the latest build <latest-build_>`_
- For patch releases, `refer to the latest runs for the branch project <evg-release_>`_.

.. warning::

   Be sure that you are looking at the correct branch in the Project Health
   page! The branch/release version will be displayed in the *Project* dropdown
   near the top of the page.

If the project health page displays task failures, ensure that they are not
unexpected by the changes introduced in the new release.


Validate that New APIs Are Documented
#####################################

The Evergreen CI task *abi-compliance-check* generates an "ABI Report"
``compat_report.html`` with an overview of all new/removed/changed symbols since
the prior release of the C driver.

Visit the most recent Evergreen build for the project, open the
*abi-compliance-check* task, go to the *Files* tab, and open the *ABI Report:
compat_report.html* artifact. In the *Added Symbols* section will be a list of
all newly introduced APIs since the most release release version. Verify that
documentation has been added for every symbol listed here. If no new symbols are
added, then the documentation is up-to-date.


Notify the PHP Driver Team
##########################

The PHP driver team consumes the C driver directly and will want to know when a
new release is coming so that they can identify regressions in the APIs used by
the PHP driver. Consider requesting that the PHP team test the PHP driver
against the new release version before the C release is tagged and published.


.. _release.github-token:

Get a GitHub API Token
######################

Later, we will use an automated script to publish the release artifacts to
GitHub and create the GitHub Release object. In order to do this, it is required
to have a GitHub API token that can be used to create and modify the releases
for the repository.

To get an access token, perform the following:

1. Open the `Settings > Personal access tokens`__ page on GitHub.
2. Press the *Generate new token* dropdown.

   1. Select a "general use"/\ "classic" token. (Creating a fine-grained access
      token requires administrative approval before it can be used.)

3. Set a *note* for the token that explains its purpose. This can be arbitrary,
   but is useful when reviewing the token later.
4. Set the expiration to the minimum (we only need the token for the duration of
   this release).
5. In the scopes, enable the ``public_repo`` scope.
6. Generate the new token. Be sure to copy the access token a save it for later,
   as it won't be recoverable once the page is unloaded.

__ https://github.com/settings/tokens

.. XXX: The following applies to fine-grained access tokens. Not sure if these work yet?

   1. Open the `Settings > Personal access tokens`__ page on GitHub.
   2. Press the *Generate new token* dropdown.

      1. Select a "Find-grained, repo-scoped" token. The general use token is also
         acceptable but is very coarse and not as restricted.

   3. Set a token name. This can be arbitrary, but would be best to refer to the
      purpose so that it can be recognized later.
   4. Set the expiration to the minimum (we only need the token for the duration of
      this release).
   5. Set the *Resource owner* to **mongodb** (**mongodb** refers to the GitHub
      organization that owns the repository that will contain the release. A
      personal account resource owner will only have access to the personal
      repositories.)
   6. Under *Repository access* choose "Only select repositories".
   7. In the repository selection dropdown, select ``mongodb/mongo-c-driver``.
   8. Under *Permissions > Repository permissions*, set the access level on
      *Contents* to *Read and write*. This will allow creating releases and
      publishing release artifacts. No other permissions need to be modified.
      (Selecting this permission may also enable the *Metadata* permission; this is
      normal.)


Do the Release
##############

.. highlight:: console
.. default-role:: bash

The release process at this point is semi-automated by scripts stored in a
separate repository.

.. hint::

   It may be useful (but is not required) perform the following steps within a
   new Python `virtual environment`__ dedicated to the process.

__ https://docs.python.org/3/library/venv.html


.. _do.stopwatch:

Start a Release Stopwatch
*************************

Start a stopwatch before proceeding.


Clone the Driver Tools
**********************

Clone the driver tools repository into a new directory, the path to which will be
called `$DRIVER_TOOLS`::

   $ git clone "git@github.com:10gen/mongo-c-driver-tools.git" $DRIVER_TOOLS

Install the Python requirements for the driver tools::

   $ pip install -r $DRIVER_TOOLS/requirements.txt


Create a New Clone of ``mongo-c-driver``
****************************************

To prevent publishing unwanted changes and to preserve local changes, create a
fresh clone of the C driver. We will clone into a new arbitrary directory which
we will refer to as `$RELEASE_CLONE`\ ::

   $ git clone "git@github.com:mongodb/mongo-c-driver.git" $RELEASE_CLONE

.. note:: Unless otherwise noted, all commands below should be executed from within
   the `$RELEASE_CLONE` directory.

Switch to a branch that corresponds to the release version:

- **If performing a minor release (x.y.0)**, create a new branch for the
  major+minor release version. For example: If the major version is ``5`` and
  the minor version is ``42``, create a branch ``r5.42``::

      $ git checkout master      # Ensure we are on the `master` branch to begin
      $ git checkout -b "r5.42"  # Create and switch to a new branch

  Push the newly created branch into the remote::

      $ git push origin "r5.42"

- **If performing a patch release (x.y.z)**, there should already exist a
  release branch corresponding to the major+minor version of the patch. For
  example, if we are releasing patch version `7.8.9`, then there should already
  exist a branch ``r7.8``. Switch to that branch now::

      $ git checkout --track origin/r7.8


**For Patch Releases**: Check Consistency with the Jira Release
***************************************************************

**If we are releasing a patch version**, we must check that the Jira release
matches the content of the branch to be released. Open
`the releases page on Jira <Jira releases_>`_ and open the release page for the new patch
release. Verify that the changes for all tickets in the Jira release have been
cherry-picked onto the release branch (not including the "Release x.y.z" ticket
that is part of every Jira release).

.. _Jira releases:
.. _jira-releases: https://jira.mongodb.org/projects/CDRIVER?selectedItem=com.atlassian.jira.jira-projects-plugin%3Arelease-page&status=unreleased


Run the Release Script
**********************

Start running the release script:

1. Let `$PREVIOUS_VERSION` be the prior ``x.y.z`` version of the C driver
   that was released.
2. Let `$NEW_VERSION` be the ``x.y.z`` version that we are releasing.
3. Run the Python script::

      $ python $DRIVER_TOOLS/release.py release $PREVIOUS_VERSION $NEW_VERSION


Fixup the ``NEWS`` Pages
************************

Manually edit the `$RELEASE_CLONE/NEWS` and `$RELEASE_CLONE/src/libbson/NEWS`
files with details of the release. **Do NOT** commit any changes to these files
yet: That step will be handled automatically by the release script in the next
steps.


Sign & Upload the Release
*************************

Run the ``release.py`` script to sign the release objects::

   $ python $DRIVER_TOOLS/release.py sign

Let `$GITHUB_TOKEN` be the personal access token that was obtained from the
:ref:`release.github-token` step above. Use the token with the ``upload`` subcommand
to post the release to GitHub:

.. note:: This will create the public release object on GitHub!

.. note:: If this is a pre-release, add the `--pre` option to the `release.py upload` command below.

::

   $ python $DRIVER_TOOLS/release.py upload $GITHUB_TOKEN

Update the ``VERSION_CURRENT`` file on the release branch::

   $ python $DRIVER_TOOLS/release.py post_release_bump


Publish Documentation
*********************

**If this is a stable release** (not a pre-release), publish the documentation
with the following command::

   $ python $DRIVER_TOOLS/release.py docs $NEW_VERSION


Announce the Release on the Community Forums
********************************************

Open the `MongoDB Developer Community / Product & Driver Announcments`__ page on
the Community Forums and prepare a new post for the release.

__ https://www.mongodb.com/community/forums/c/announcements/35

To generate the release template text, use the following::

   $ python $DRIVER_TOOLS/release.py announce -t community $NEW_VERSION

Update/fix-up the generated text for the new release and publish the new post.

.. seealso::

   `An example of a release announcment post`__

   __ https://www.mongodb.com/community/forums/t/mongodb-c-driver-1-24-0-released/232021


Copy the Release Updates to the ``master`` Branch
*************************************************

Create a new branch from the C driver ``master`` branch, which will be used to
publish a PR to merge the updates to the release files back into ``master``::

   $ git checkout master
   $ git checkout post-release-merge

(Here we have named the branch ``post-release-merge``, but the branch name is
arbitrary.)

Manually update the ``NEWS``, ``src/libbson/NEWS``, and ``VERSION_CURRENT``
files with the content from the release branch that we just published. Commit
these changes to the new branch.

Push this branch to your fork of the repository::

   $ git push git@github.com:$YOUR_GH_USERNAME/mongo-c-driver.git post-release-merge

Now `create a new GitHub Pull Request`__ to merge the ``post-release-merge``
changes back into the ``master`` branch.

__ https://github.com/mongodb/mongo-c-driver/pulls


Close the Jira Release Ticket and Finish the Jira Release
*********************************************************

Return to the `Jira releases`_ page and open the release for the release
version. Close the *Release x.y.z* ticket that corresponds to the release and
"Release" that version in Jira, ensuring that the release date is correct. (Do
not use the "Build and Release" option)


Comment on the Generated DOCSP Ticket
*************************************

.. note:: This step is not applicable for patch releases.

After a **minor** or **major** release is released in Jira (done in the previous
step), a DOCSP "Update Compat Tables" ticket will be created automatically
(`example DOCSP ticket`__). Add a comment to the newly created ticket for the
release describing if there are any changes needed for the
`driver/server compatibility matrix`__ or the
`C language compatibility matix`__.

__ https://jira.mongodb.org/browse/DOCSP-39145
__ https://www.mongodb.com/docs/languages/c/c-driver/current/#mongodb-compatibility
__ https://www.mongodb.com/docs/languages/c/c-driver/current/#language-compatibility


Update the Release Evergreen Project
************************************

**For minor releases**, open the
`release project settings <evg-release-settings_>`_ and update the *Display
Name* and *Branch Name* to match the new major+minor release version.


Stop the Stopwatch & Record the Release
***************************************

Stop the stopwatch started at :ref:`do.stopwatch`. Record the the new release
details in the `C/C++ Release Info`__ sheet.

__ https://docs.google.com/spreadsheets/d/1yHfGmDnbA5-Qt8FX4tKWC5xk9AhzYZx1SKF4AD36ecY/edit#gid=0


Homebrew Release
################

.. note::

   This step requires a macOS machine. If you are not using macOS, ask in the
   ``#dbx-c-cxx`` channel for someone to do this step on your behalf.

**If this is a stable release**, update `the mongo-c-driver homebew formula`__. Let
`$ARCHIVE_URL` be the URL to the release tag's source archive on GitHub\ [#tar-url]_::

   $ brew bump-formula-pr mongo-c-driver --url $ARCHIVE_URL

__ https://github.com/Homebrew/homebrew-core/blob/master/Formula/m/mongo-c-driver.rb

.. [#tar-url] For example, the tagged archive for ``1.25.0`` is at https://github.com/mongodb/mongo-c-driver/archive/refs/tags/1.25.0.tar.gz


Linux Distribution Packages
###########################

.. ! NOTE: Updates to these instructions should be synchronized to the corresponding
   ! C++ release process documentation located in the "etc/releasing.md" file in the C++
   ! driver repository


Debian
******

.. note::

   If you are not a Debian maintainer on the team, consider opening a new
   CDRIVER Jira ticket for another team member to do the Debian release.
   `Example Debian release ticket`__

   __ https://jira.mongodb.org/browse/CDRIVER-4761

To publish a new release Debian package, perform the following:

1. For the first Debian package release on a **new release branch**, edit
   ``debian/gbp.conf`` and update the ``upstream-branch`` and ``debian-branch``
   variables to match the name of the new release branch (e.g., ``r1.xx``); both
   variables should have the same value.

   .. note:: The Debian package release is made after the upstream release has been tagged

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


Fedora
******

After the changes for `CDRIVER-3957`__, the RPM spec file has been vendored into
the project; it needs to be updated periodically. The DBX C/C++ team does not
maintain the RPM spec file. These steps can be done once the RPM spec file is
updated (which will likely occur some time after the C driver is released).

__ https://jira.mongodb.org/browse/CDRIVER-3957

1. From the project's root directory, download the latest spec file::

      $ curl -L -o .evergreen/mongo-c-driver.spec https://src.fedoraproject.org/rpms/mongo-c-driver/raw/rawhide/f/mongo-c-driver.spec

2. Confirm that our spec patch applies to the new downstream spec::

      $ patch --dry-run -d .evergreen/etc -p0 -i spec.patch

3. If the patch command fails, rebase the patch on the new spec file.
4. For a new major release (e.g., 1.17.0, 1.18.0, etc.), then ensure that the
   patch updates the `up_version` to be the NEXT major version (e.g., when
   releasing 1.17.0, the spec patch should update `up_version`` to 1.18.0); this
   is necessary to ensure that the spec file matches the tarball created by the
   dist target; if this is wrong, then the `rpm-package-build` task will fail in
   the next step.
5. Additionally, ensure that any changes made on the release branch vis-a-vis
   the spec file are also replicated on the master or main branch.
6. Test the RPM build in Evergreen with a command such as the following::

      $ evergreen patch -p mongo-c-driver -v packaging -t rpm-package-build -f

7. There is no package upload step, since the downstream maintainer handles that
   and we only have the Evergreen task to ensure that we do not break the
   package build.
8. The same steps need to be executed on active release branches (e.g., r1.19),
   which can usually be accomplished via `git cherry-pick` and then resolving
   any minor conflict.


vcpkg
#####

To update the package in vcpkg, create an issue to update
`the mongo-c-driver manifest`__. To submit an issue, `follow the steps here`__
(`example issue`__).

Await a community PR to resolve the issue, or submit a new PR.

__ https://github.com/microsoft/vcpkg/blob/master/versions/m-/mongo-c-driver.json
__ https://github.com/microsoft/vcpkg/issues/new/choose
__ https://github.com/microsoft/vcpkg/issues/34855


Conan
#####

Create a new issue in the conan-center-index project to update `the recipe files
for the C driver package`__. To submit an issue, `follow the process
here`__ (`example issue`__)

Await a community PR to resolve the issue, or submit a new PR.

__ https://github.com/conan-io/conan-center-index/blob/master/recipes/mongo-c-driver/config.yml
__ https://github.com/conan-io/conan-center-index/issues/new/choose/
__ https://github.com/conan-io/conan-center-index/issues/20879


Docker
######

The C driver does not have its own container image, but it may be useful to
update the C driver used in the C++ container image build.

If the C driver is being released without a corresponding C++ driver release, consider
updating the mongo-cxx-driver container image files to use the newly released C driver
version. `Details for this process are documented here`__

__ https://github.com/mongodb/mongo-cxx-driver/blob/5f2077f98140ea656983ea5881de31d73bb3f735/etc/releasing.md#docker-image-build-and-publish

