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

from evergreen_config_generator import ConfigObject


class Variant(ConfigObject):
    def __init__(self, name, display_name, run_on, tasks, expansions=None,
                 batchtime=None):
        super(Variant, self).__init__()
        self._variant_name = name
        self.display_name = display_name
        self.run_on = run_on
        self.tasks = tasks
        self.expansions = expansions
        self.batchtime = batchtime

    @property
    def name(self):
        return self._variant_name

    def to_dict(self):
        v = super(Variant, self).to_dict()
        for i in 'display_name', 'expansions', 'run_on', 'tasks', 'batchtime':
            if getattr(self, i):
                v[i] = getattr(self, i)
        return v
