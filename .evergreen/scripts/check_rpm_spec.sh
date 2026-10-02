#!/usr/bin/env bash

#
# Copyright 2009-present MongoDB, Inc.
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

set -o errexit

#
# check_rpm_spec.sh - Check if our RPM spec matches downstream's
#
# Supported/used environment variables:
#   IS_PATCH    If "true", this is an Evergreen patch build.


on_exit () {
  if [ -n "${SPEC_FILE}" ]; then
    rm -f "${SPEC_FILE}"
  fi
}
trap on_exit EXIT

if [ "${IS_PATCH}" = "true" ]; then
   echo "This is a patch build...skipping RPM spec check"
   exit
fi

SPEC_FILE=$(mktemp --tmpdir -u mongo-c-driver.XXXXXXXX.spec)
# Check against the spec file for the EPEL9 branch, as that is where 1.30.x releases are landed
curl --retry 5 https://src.fedoraproject.org/rpms/mongo-c-driver/raw/epel9/f/mongo-c-driver.spec -sS --max-time 120 --fail --output "${SPEC_FILE}"

diff -q .evergreen/etc/mongo-c-driver.spec "${SPEC_FILE}" || \
   (
   echo "Synchronize RPM spec from downstream to fix this failure.";
   echo "Instructions:";
   echo "1. Download spec file from https://src.fedoraproject.org/rpms/mongo-c-driver/raw/epel9/f/mongo-c-driver.spec";
   echo "2. Replace spec file at .evergreen/etc/";
   echo "3. Update .evergreen/etc/spec.patch (diff should increment minor version on master branch and patch version on release branch)";
   echo "Examples: (master) 9873322a98a2f1c67b6da6da4c9a2ac573799ea5 / (release branch) 7c2c27be7f9f50dd026c90a4491027f0f0dd6753";
   exit 1
   )
