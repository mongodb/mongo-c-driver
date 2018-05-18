#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

# Check that a CLion user didn't accidentally convert NEWS from UTF-8 to ASCII
news_type=`file NEWS`
echo "NEWS file type is $news_type"
case "$news_type" in
  *ASCII*) exit 1 ;;
esac

# Use modern sphinx-build from venv.
. venv/bin/activate
which sphinx-build
sphinx-build --version

DIR=$(dirname $0)
. $DIR/find-cmake.sh

$CMAKE -DENABLE_MAN_PAGES=ON -DENABLE_HTML_DOCS=ON -DENABLE_ZLIB=BUNDLED -DENABLE_BSON=ON .
make DISTCHECK_BUILD_OPTS="-j 8" distcheck

# Check that docs were included, but sphinx temp files weren't.
tarfile=mongo-c-driver-*.tar.gz
docs='mongo-c-driver-*/doc/html/index.html mongo-c-driver-*/doc/man/mongoc_client_t.3'
tmpfiles='mongo-c-driver-*/doc/html/.doctrees \
   mongo-c-driver-*/doc/html/.buildinfo \
   mongo-c-driver-*/doc/man/.doctrees \
   mongo-c-driver-*/doc/man/.buildinfo'

echo "Checking for built docs"
for doc in $docs; do
   # Check this doc is in the archive.
   tar --wildcards -tzf $tarfile $doc
done

echo "Checking that temp files are not included in tarball"
for tmpfile in $tmpfiles; do
   # Check this temp file doesn't exist.
   if tar --wildcards -tzf $tarfile $tmpfile > /dev/null 2>&1; then
      echo "Found temp file in archive: $tmpfile"
      exit 1
   fi
done

echo "Checking that index.3 wasn't built"
if tar --wildcards -tzf $tarfile 'mongo-c-driver-*/doc/man/index.3' > /dev/null 2>&1; then
   echo "Found index.3 in archive"
   exit 1
fi

echo "Checking that all C files are included in tarball"
# Check that all C files were included.
TAR_CFILES=`tar --wildcards -tf mongo-c-driver-*.tar.gz 'mongo-c-driver-*/src/libmongoc/src/mongoc/*.c' | cut -d / -f 4 | sort`
SRC_CFILES=`echo src/libmongoc/src/mongoc/*.c | xargs -n 1 | cut -d / -f 3 | sort`
if [ "$TAR_CFILES" != "$SRC_CFILES" ]; then
   echo "Not all C files are in the release archive"
   echo $TAR_CFILES > tar_cfiles.txt
   echo $SRC_CFILES | diff -y - tar_cfiles.txt
fi

