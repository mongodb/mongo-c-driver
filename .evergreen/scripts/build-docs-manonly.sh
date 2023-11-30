#!/usr/bin/env bash

set -o errexit # Exit the script with error if any of the commands fail

. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths # For MONGOC_DIR
. "$(dirname "${BASH_SOURCE[0]}")/find-cmake-latest.sh"
CMAKE=$(find_cmake_latest)


debug "Calculating release version..."
python build/calc_release_version.py >VERSION_CURRENT
python build/calc_release_version.py -p >VERSION_RELEASED

build_dir=$MONGOC_DIR/_build/for-docs
"$CMAKE" -S "$MONGOC_DIR" -B "$build_dir" \
  -D ENABLE_MAN_PAGES=ON \
  -D ENABLE_ZLIB=BUNDLED
"$CMAKE" --build "$build_dir" \
  --parallel 8 \
  --target bson-doc \
  --target mongoc-doc
