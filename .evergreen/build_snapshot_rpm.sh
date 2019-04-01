#!/bin/sh

set -o xtrace
set -o errexit

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
    echo "  requires utilites from these packages: rpm-build, mock, wget"
    echo ""
    exit
  fi
done

package=mongo-c-driver
spec_file=../mongo-c-driver.spec
spec_url=https://src.fedoraproject.org/rpms/mongo-c-driver/raw/master/f/mongo-c-driver.spec
config=${MOCK_TARGET_CONFIG:=fedora-28-x86_64}

if [ ! -x /usr/bin/rpmbuild -o ! -x /usr/bin/rpmspec ]; then
  echo "Missing the rpmbuild or rpmspec utility from the rpm-build package"
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

build_dir=$(basename $(pwd))

sudo mock -r ${config} --bootstrap-chroot --old-chroot --clean
sudo mock -r ${config} --bootstrap-chroot --old-chroot --init
mock_root=$(sudo mock -r ${config} --bootstrap-chroot --old-chroot --print-root-path)
sudo mock -r ${config} --bootstrap-chroot --old-chroot --install rpmdevtools git rpm-build cmake python python2-sphinx
sudo mock -r ${config} --bootstrap-chroot --old-chroot --copyin "$(pwd)" "$(pwd)/${spec_file}" /tmp
if [ ! -f VERSION_CURRENT ]; then
  sudo mock -r ${config} --bootstrap-chroot --old-chroot --cwd "/tmp/${build_dir}" --chroot -- /bin/sh -c "(
    set -o xtrace ;
    python build/calc_release_version.py | sed -E 's/([^-]+).*/\1/' > VERSION_CURRENT ;
    python build/calc_release_version.py -p > VERSION_RELEASED
    )"
  sudo mock -r ${config} --bootstrap-chroot --old-chroot --copyout "/tmp/${build_dir}/VERSION_CURRENT" "/tmp/${build_dir}/VERSION_RELEASED" .
fi

bare_upstream_version=$(sed -E 's/([^-]+).*/\1/' VERSION_CURRENT)
# Upstream version in the .spec file cannot have hyphen (-); replace the current
# version so that the dist tarball version does not have a pre-release component
echo ${bare_upstream_version} > VERSION_CURRENT
echo "Found bare upstream version: ${bare_upstream_version}"
git_rev="$(git rev-parse --short HEAD)"
snapshot_version="${bare_upstream_version}-0.$(date +%Y%m%d)+git${git_rev}"
echo "Upstream snapshot version: ${snapshot_version}"
current_package_version=$(rpmspec --srpm -q --qf "%{version}-%{release}" ${spec_file})

if [ -n "${current_package_version##*${git_rev}*}" ]; then
  echo "Making RPM changelog entry"
  sudo mock -r ${config} --bootstrap-chroot --old-chroot --cwd "/tmp/${build_dir}" --chroot -- rpmdev-bumpspec --comment="Built from Git Snapshot." --userstring="Test User <test@example.com>" --new="${snapshot_version}%{?dist}" ${spec_file}
fi

sudo mock -r ${config} --bootstrap-chroot --old-chroot --copyout "/tmp/${build_dir}/${spec_file}" ..

sudo mock -r ${config} --bootstrap-chroot --old-chroot --cwd "/tmp/${build_dir}" --chroot -- /bin/sh -c "(
  set -o xtrace ;
  [ -d cmake-build ] || mkdir cmake-build ;
  cd cmake-build ;
  /usr/bin/cmake -DENABLE_MAN_PAGES=ON -DENABLE_HTML_DOCS=ON -DENABLE_ZLIB=BUNDLED -DENABLE_BSON=ON .. ;
  make -j 8 dist
  )"

[ -d cmake-build ] || mkdir cmake-build
sudo mock -r ${config} --bootstrap-chroot --old-chroot --copyout "/tmp/${build_dir}/cmake-build/${package}*.tar.gz" cmake-build

[ -d ~/rpmbuild/SOURCES ] || mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
mv cmake-build/${package}*.tar.gz ~/rpmbuild/SOURCES/

echo "Building source RPM ..."
rpmbuild -bs ${spec_file}
echo "Building binary RPMs ..."
mock_result=$(readlink -f ../mock-result)
sudo mock --resultdir="${mock_result}" --bootstrap-chroot --old-chroot -r ${config} --no-clean --no-cleanup-after --rebuild ~/rpmbuild/SRPMS/${package}-${snapshot_version}*.src.rpm
sudo mock -r ${config} --bootstrap-chroot --old-chroot --copyin "${mock_result}" /tmp

sudo mock -r ${config} --bootstrap-chroot --old-chroot --cwd "/tmp/${build_dir}" --chroot -- /bin/sh -c "(
  set -o xtrace &&
  rpm -Uvh ../mock-result/*.rpm &&
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0
  )"

if [ ! -e "${mock_root}/tmp/${build_dir}/example-client" ]; then
  echo "Example was not built!"
  sudo mock -r ${config} --bootstrap-chroot --old-chroot --clean
  exit 1
fi

sudo mock -r ${config} --bootstrap-chroot --old-chroot --clean
(cd "${mock_result}" ; tar zcvf ../rpm.tar.gz *.rpm)

