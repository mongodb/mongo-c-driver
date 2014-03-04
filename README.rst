=========
libmongoc
=========

About
=====

Libmongoc is a client library written in C for MongoDB.

There are absolutely no guarantees of API/ABI stability at this point.
But generally, we won't break API/ABI unless we have good reason.

Libmongoc depends on `Libbson <https://github.com/mongodb/libbson>`_.

If you are looking for the legacy C driver, it can be found in the
`legacy branch <https://github.com/mongodb/mongo-c-driver/tree/legacy>`_.

Support / Feedback
==================

For issues with, questions about, or feedback for libmongoc, please look into
our `support channels <http://www.mongodb.org/about/support>`_. Please
do not email any of the libmongoc developers directly with issues or
questions - you're more likely to get an answer on the `mongodb-user
<http://groups.google.com/group/mongodb-user>`_ list on Google Groups.

Bugs / Feature Requests
=======================

Think you’ve found a bug? Want to see a new feature in libmongoc? Please open a
case in our issue management tool, JIRA:

- `Create an account and login <https://jira.mongodb.org>`_.
- Navigate to `the CDRIVER project <https://jira.mongodb.org/browse/CDRIVER>`_.
- Click **Create Issue** - Please provide as much information as possible about the issue type and how to reproduce it.

Bug reports in JIRA for all driver projects (i.e. CDRIVER, CSHARP, JAVA) and the
Core Server (i.e. SERVER) project are **public**.

Security Vulnerabilities
------------------------

If you’ve identified a security vulnerability in a driver or any other
MongoDB project, please report it according to the `instructions here
<http://docs.mongodb.org/manual/tutorial/create-a-vulnerability-report>`_.


Building From Git
=================

The following example will install both libbson and mongo-c-driver from git.
It assumes you do not yet have libbson installed and are on a 64-bit system.

Dependencies
------------

Fedora::

  $ sudo yum install git gcc automake autoconf libtool

FreeBSD::

  $ pkg install git gcc automake autoconf libtool


Clone Repositories
------------------

You can use the following to checkout and build mongo-c-driver.::

  $ git clone https://github.com/mongodb/mongo-c-driver.git
  $ cd libbson
  $ ./autogen.sh
  $ make
  $ sudo make install

In standard automake fasion, ./autogen.sh only needs to be run once.
You can use ./configure directly going forward.
Also see ./configure --help for all configure options.
