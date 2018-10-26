# Copyright 2018-present MongoDB, Inc.
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

import sys
from collections import OrderedDict as OD

try:
    import yaml
    import yamlordereddictloader
except ImportError:
    sys.stderr.write("try 'pip install -r build/requirements.txt'")
    raise


# We want legible YAML tasks:
#
#     - name: debug-compile
#       tags: [zlib, snappy, compression, openssl]
#       commands:
#       - command: shell.exec
#         params:
#           script: |-
#             set -o errexit
#             set -o xtrace
#             ...
#
# Write values compactly except multiline strings, which use "|" style. Write
# tag sets as lists.

class Dumper(yamlordereddictloader.Dumper):
    def __init__(self, *args, **kwargs):
        super(Dumper, self).__init__(*args, **kwargs)
        self.add_representer(set, type(self).represent_set)

    def represent_scalar(self, tag, value, style=None):
        if isinstance(value, (str, unicode)) and '\n' in value:
            style = '|'
        return super(Dumper, self).represent_scalar(tag, value, style)

    def represent_set(self, data):
        return super(Dumper, self).represent_list(sorted(data))


class ConfigObject(object):
    def __init__(self, *args, **kwargs):
        super(ConfigObject, self).__init__()

    @property
    def name(self):
        return 'UNSET'

    def to_dict(self):
        return OD([('name', self.name)])

    def to_yaml(self):
        return yaml.dump(self.to_dict(), Dumper=Dumper)
