#!/usr/bin/env python

import argparse
import datetime
import json
import re
import string
import sys
from typing import Any, Callable, Iterable, Sequence, TypeVar


T = TypeVar("T")
U = TypeVar("U")
V = TypeVar("V")


def _atop(f: Callable[[U], V], g: Callable[[T], U]) -> Callable[[T], V]:
    """Create a composed function (f ∘ g)"""
    return lambda x: f(g(x))


FULL_TEMPLATE = string.Template(
    r"""
<!-- This file was GENERATED using snyk-vulns.py -->
# Thirty-Party Vulnerabilities

This document lists known vulnerabilities in third-party dependencies that are
directly bundled with standard release product for the MongoDB C Driver and
libbson libraries.

This document was created on **$today** using data from
[Snyk Security](https://security.snyk.io), and the details herein reflect
information that was available at that time.

> [!IMPORTANT]
>
> The "standard release product" is defined as the set of files which are
> _installed_ by a configuration, build, and install. This includes
> static/shared library files, header files, and packaging files for supported
> build configurations.
>
> Vulnerabilities for 3rd party dependencies that are bundled with the standard
> release product are reported in this document. Test files, utility scripts,
> documentation generators, and other miscellaneous files and artifacts are NOT
> considered part of the standard release product, even if they are included in
> the release distribution tarball. Vulnerabilities for such 3rd party
> dependencies are NOT reported in this document.
>
> Details on packages that are not tracked tracked by Snyk Security will not
> appear in this document.


# Vulnerabilities

$vulns

---

[^bundled-version]: The *bundled version* attribute corresponds to the version of
    the dependency that is bundled with the release, not necessarily the version
    where the vulnerability first appeared.

[^fix-version]: The *fix versions* attribute refers to the versions of the dependency
    package where the vulnerability has been addressed and removed.
"""
)


VULN_TEMPLATE = string.Template(
    R"""
## $package - $cve - $title

- Snyk Security Entry: [$snyk_id](https://security.snyk.io/vuln/$snyk_id)
- CVE Record: [$cve](https://www.cve.org/CVERecord?id=$cve)
- Package: $package
- Disclosed: $disclose_date
- Published: $pub_date
- Severity: $severity
- Bundled Version: $found_version [^bundled-version]
- Fix Versions: $fix_version [^fix-version]

<!-- External content from $snyk_id -->
$desc
<!-- End external content -->
"""
)

NONE_DETECTED = """
> No issues in any external libraries were detected at the time of this release
"""


def _vuln_attributes(iss: Any) -> dict[str, str]:
    """
    Match Snyk vulnerability JSON data and extract the attributes that we
    wish to include in the report. The returned attribute keys correspond to
    the substitutions to be performed in the vulnerability template string.
    """
    match iss:
        case {
            "identifiers": {"CVE": [cve]},
            "title": title,
            "packageName": pkg,
            "disclosureTime": disclosed,
            "id": snyk_id,
            "publicationTime": published,
            "severity": severity,
            "version": version,
            "fixedIn": fixed,
            "description": str(desc),
        }:
            disclosed = datetime.datetime.fromisoformat(disclosed)
            published = datetime.datetime.fromisoformat(published)
            # The "description" is Markdown-style text. Add nesting levels to section headings.
            deeper_desc = "\n".join(
                (f"##{line}" if line.startswith("#") else line)
                for line in desc.splitlines()
            )
            return {
                "cve": cve,
                "title": title,
                "package": pkg,
                "severity": string.capwords(severity),
                "found_version": version,
                "fix_version": ", ".join(fixed) or "(None yet)",
                "disclose_date": f"{disclosed:%B %-d, %Y}",
                "pub_date": f"{published:%B %-d, %Y}",
                "desc": deeper_desc,
                "snyk_id": snyk_id,
            }
        case _:
            raise RuntimeError(
                f"Vulnerability entry did not match the expected pattern during rendering (Got {iss=}). "
                f"Update the pattern matching to handle this item."
            )


render_vuln = _atop(VULN_TEMPLATE.substitute, _vuln_attributes)


def _split_cve_str(s: str) -> Iterable[str]:
    """Splits a comma-separated list of CVE IDs"""
    if not s:
        return  # Empty string; Empty list.
    cve_re = re.compile(r"^CVE-\d{4}-\d+$")
    split = s.split(",")
    trimmed = map(str.strip, split)
    for cve in trimmed:
        if not cve_re.match(cve):
            raise ValueError(f"Invalid CVE identifier {cve=}")
        yield cve


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(
        description="""
        Generates a vulnerabilitiy report Markdown file from Snyk JSON data. The Snyk
        data is read from stdin, and the resulting Markdown is written to stdout.
        """,
        allow_abbrev=False,
    )
    parser.add_argument(
        "--cve-exclude",
        metavar="CVE-YYYY-NNNN[,…]",
        help="Comma-separated list of CVE entries that should not be included in the generated report",
        type=_atop(set, _split_cve_str),
        default=set(),
    )
    args = parser.parse_args(argv)
    # Read Snyk JSON from stdin
    data = json.load(sys.stdin)[0]
    # Extract the vulnerabilities items
    vuln_data = data["vulnerabilities"]
    kept_vulns = list(
        v
        for v in vuln_data
        # Ignore vulnerabilities if their CVE numbers are any CVE in the --cve-exclude list
        if not set(v["identifiers"]["CVE"]).intersection(args.cve_exclude)
    )
    # Format the current date as "<month> <day>, <year>"
    today = f"{datetime.datetime.now():%B %-d, %Y}"
    # Format each vuln entry, joined with two newlines
    each_rendered = map(render_vuln, kept_vulns)
    vulns_str = "\n\n".join(each_rendered)
    # Render the whole doc
    full = FULL_TEMPLATE.substitute(vulns=vulns_str or NONE_DETECTED, today=today)
    # Print to stdout
    sys.stdout.write(full)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
