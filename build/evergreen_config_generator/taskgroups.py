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


class TaskGroup(ConfigObject):
    def __init__(self, name):
        super(TaskGroup, self).__init__()
        self._task_group_name = name

    @property
    def name(self):
        return self._task_group_name

    def to_dict(self):
        v = super(TaskGroup, self).to_dict()
        # See possible TaskGroup attributes from the Evergreen wiki:
        # https://github.com/evergreen-ci/evergreen/wiki/Project-Configuration-Files#task-groups
        attrs = [
            'setup_group',
            'teardown_group',
            'setup_task',
            'teardown_task',
            'max_hosts',
            'timeout',
            'setup_group_can_fail_task',
            'setup_group_timeout_secs',
            'setup_group_can_fail_task',
            'share_processes',
            'tasks'
        ]        
                 
        for i in attrs:
            if getattr(self, i, None):
                v[i] = getattr(self, i)
        return v
        