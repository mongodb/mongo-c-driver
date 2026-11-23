Credential disclosure corpus
============================

``credential-disclosure.json`` is copied without modification from the
DRIVERS-3623 connection-string tests. The cases include credential arguments
that fail integer conversion or name an unrecognized authentication mechanism.

The dedicated runner in ``test-mongoc-connection-uri.c`` exercises both URI
constructors without a server. It checks error messages on rejection and
captured logs at every enabled level on both acceptance and rejection for the
literal and fully percent-encoded sentinel, case-insensitively. Acceptance and
parsed components are deliberately not asserted by this corpus.

Additional driver tests check omission of non-secret input, canonical option and
authentication mechanism names, formatted integer arguments of known,
non-sensitive options, query-relative character positions, and TXT option
precedence. String-representation API changes are outside this diagnostic proof
of concept.
