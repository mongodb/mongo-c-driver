# `mongo-c-tools`

This directory contains shared scripts and tools for native C and C++
development.


## As a Subrepo

If this directory contains a `.gitrepo` file, then this directory is being
tracked as a *subrepo* of some parent repository. If this file does not exist
but a `.git/` directory *does* exist, then you are looking at the tools
repository itself.

[`git subrepo`](https://github.com/ingydotnet/git-subrepo) is a set of scripts
and conventions for pushing/pulling/syncing changes in a subdirectory between
different repositories with full history tracking. *Using* the contents of this
directory does not require the `git subrepo` tool, but pushing and pulling
changes in this subrepo directory will require `git subrepo` (or knowing
the appropriate commands to execute for the same effect).

If you are simply using these scripts, you do not have to deal with the subrepo
commands. If you wish to modify/sync these scripts, read on below.


### Preparing

`git-subrepo` tracks a subrepo directory using a remote+branch pair as the
"source of truth" for the subrepo contents. The remote+branch pair is fixed in
the `.gitrepo` file. In this subrepo directory, the `remote` is not set to a
URL, but to a name `tools-origin`. In order to push/pull, you will need to
ensure that you have a `tools-origin` set to the proper URL. Unless you "know
what you are doing", you will want to use the upstream `mongo-c-driver`
repository as the remote:

```sh
$ git remote add tools-origin "git@github.com:mongodb/mongo-c-driver.git"
```

The content of this subrepo is contained by the disjoint branch `mongo-c-tools`,
which contains only the history of this directory as the root.

**NOTE** that doing a `git subrepo push` will send changes into the remote
branch immediately without a PR! See below about how to push changes.


### Pulling Changes

If the remote copy of the subrepo has been updated (e.g. as part of another
project's changes), then the local copy of this subrepo can be updated by using
`git subrepo pull`:

```sh
$ git subrepo pull $this_subdir
```

**Note:**

- Doing a `subrepo pull` requires that there be *no* unstaged changes in the
  parent repository.
- Pulling a subrepo will create a new commit on the current branch. This commit
  contains the changes that were pulled from the remote, as well as an update to
  the `.gitrepo` file.
- If merge conflicts occur, you will find yourself in a copy of the disjoint
  branch in order to perform a conflict resolution. Follow the instructions
  from `git-subrepo` to handle the merge.


### Modifying these Files

Before modifying the contents of this directory, be sure that you have the
latest changes from the remote branch according to the
[*pulling changes*](#pulling-changes) section. This will avoid annoying merge
conflicts.

To modify the contents of this directory, simply update and commit them as one
would do normally. If you want your changes to undergo review:

1. Create a PR and go through code review as normal. Don't worry about the
   subrepo at this step.
2. Merge the PR as normal.
3. Begin a new temporary branch `$tmp` for the subrepo update that points to the
   merge commit that resulted from the PR merge.
4. With the `$tmp` branch active, run `git subrepo push`. This will create a new
   commit on `$tmp` and send the local changes into the remote.
5. Run `git subrepo status` and verify that the `Pull Parent` of the subrepo
   refers to the merge commit from the orginal PR.
6. Merge the `$tmp` back into the base branch, either through a PR or by just
   pushing it directly without a PR (the `git subrepo push` will only generate a
   few lines of change in the `.gitrepo` file and won't be interesting to
   review).
