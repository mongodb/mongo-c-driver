#!/bin/sh

#
# build_snapshot_rpm.sh
#

#
# Copyright 2018 MongoDB, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


for arg in "$@"; do
  if [ "$arg" = "-h" ]; then
    echo "Usage: ./.evergreen/build_snapshot_rpm.sh"
    echo ""
    echo "  This script is used to build a .rpm package directly from a snapshot of the"
    echo "  current repository."
    echo ""
    echo "  This script must be called from the base directory of the repository, and"
    echo "  requires utilites from these packages: rpmdevtools, rpm-build, mock, wget"
    echo ""
    exit
  fi
done

package=mongo-c-driver
spec_file=../mongo-c-driver.spec
spec_url=https://src.fedoraproject.org/rpms/mongo-c-driver/raw/master/f/mongo-c-driver.spec
config=${MOCK_TARGET_CONFIG:=fedora-28-x86_64}

if [ ! -x /usr/bin/rpmdev-bumpspec ]; then
  echo "Missing the rpmdev-bumpspec utility from the rpmdevtools package"
  exit 1
fi

if [ ! -x /usr/bin/rpmbuild -o ! -x /usr/bin/rpmspec ]; then
  echo "Missing the rpmbuild or rpmspec utility from the rpm-build package"
  exit 1
fi

if [ ! -f VERSION_CURRENT ]; then
  echo "This script must be called from the base directory of the package"
  exit 1
fi

if [ ! -d .git ]; then
  echo "This script only works from within a repository"
  exit 1
fi

if [ ! -x /usr/bin/mock ]; then
  echo "Missing mock"
  exit 1
fi

if [ ! -x /usr/bin/wget ]; then
  echo "Missing wget"
  exit 1
fi

if [ -f "${spec_file}" ]; then
  echo "Found old spec file (${spec_file})...removing"
  rm -f  ${spec_file}
fi
/usr/bin/wget -O "${spec_file}" "${spec_url}"
if [ "${?}" != "0" -o ! -f "${spec_file}" ]; then
  echo "Could not retrieve spec file from URL: ${spec_url}"
  exit 1
fi

changelog_package=$(rpmspec --srpm -q --qf "%{name}" ${spec_file})
if [ "${package}" != "${changelog_package}" ]; then
  echo "This script is configured to create snapshots for ${package} but you are trying to create a snapshot for ${changelog_package}"
  exit 1
fi

bare_upstream_version=$(sed -E 's/([^-]+).*/\1/' VERSION_CURRENT)
# Upstream version in the .spec file cannot have hyphen (-); replace the current
# version so that the dist tarball version does not have a pre-release component
echo ${bare_upstream_version} > VERSION_CURRENT
echo "Found bare upstream version: ${bare_upstream_version}"
git_rev="$(git rev-parse --short HEAD)"
snapshot_version="${bare_upstream_version}-0.$(date +%Y%m%d).git${git_rev}"
echo "Upstream snapshot version: ${snapshot_version}"
current_package_version=$(rpmspec --srpm -q --qf "%{version}-%{release}" ${spec_file})

if [ -n "${current_package_version##*${git_rev}*}" ]; then
  echo "Making RPM changelog entry"
  rpmdev-bumpspec --comment="Built from Git Snapshot." --userstring="Test User <test@example.com>" --new="${snapshot_version}%{?dist}" ${spec_file}
fi

DIR=$(dirname $0)
. $DIR/find-cmake.sh

mkdir cmake-build
pushd cmake-build
$CMAKE -DENABLE_MAN_PAGES=ON -DENABLE_HTML_DOCS=ON -DENABLE_ZLIB=BUNDLED -DENABLE_BSON=ON ..
make -j 8 dist
popd

[ -d ~/rpmbuild/SOURCES ] || mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
mv cmake-build/${package}*.tar.gz ~/rpmbuild/SOURCES/

echo "Building source RPM ..."
rpmbuild -bs ${spec_file}
echo "Building binary RPMs ..."
sudo mock --rootdir="$(readlink -f ../mock-root)" --resultdir="$(readlink -f ../mock-result)" -r ${config} --rebuild ~/rpmbuild/SRPMS/${package}-${snapshot_version}*.src.rpm

