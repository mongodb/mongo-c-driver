"""Build the C client docs.
"""

from __future__ import with_statement
import os
import shutil
import socket
import subprocess
import time
import urllib2


def clean_dir(dir):
    try:
        shutil.rmtree(dir)
    except:
        pass
    os.makedirs(dir)

def gen_c(dir):
    clean_dir(dir)
    clean_dir("docs/doxygen")

    # Too noisy...
    with open("/dev/null") as null:
        subprocess.call(["doxygen", "doxygenConfig"], stdout=null, stderr=null)

    os.rename("docs/doxygen/html", dir)


def version():
    """Get the driver version from doxygenConfig.
    """
    with open("doxygenConfig") as f:
        for line in f.readlines():
            if line.startswith("PROJECT_NUMBER"):
                return line.split("=")[1].strip()


def main():
    v = version()
    print("Generating C docs in docs/html/c/%s" % v)
    gen_c("docs/html/c/%s" % v)


if __name__ == "__main__":
    main()


