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

from collections import OrderedDict as OD
from itertools import product

try:
    # Python 3 abstract base classes.
    import collections.abc as abc
except ImportError:
    import collections as abc

from evergreen_config_generator import ConfigObject
from evergreen_config_generator.functions import func


class Task(ConfigObject):
    def __init__(self, *args, **kwargs):
        super(Task, self).__init__(*args, **kwargs)
        self.tags = set()
        self.options = OD()
        self.depends_on = None
        self.commands = kwargs.pop('commands', None) or []
        tags = kwargs.pop('tags', None)
        if tags:
            self.add_tags(*tags)
        depends_on = kwargs.pop('depends_on', None)
        if depends_on:
            self.add_dependency(depends_on)

        if 'exec_timeout_secs' in kwargs:
            self.options['exec_timeout_secs'] = kwargs.pop('exec_timeout_secs')

    name_prefix = 'test'

    def add_tags(self, *args):
        self.tags.update(args)

    def has_tags(self, *args):
        return bool(self.tags.intersection(args))

    def add_dependency(self, dependency):
        if not isinstance(dependency, abc.Mapping):
            dependency = OD([('name', dependency)])

        if self.depends_on is None:
            self.depends_on = dependency
        elif isinstance(self.depends_on, abc.Mapping):
            self.depends_on = [self.depends_on, dependency]
        else:
            self.depends_on.append(dependency)

    def display(self, axis_name):
        value = getattr(self, axis_name)
        # E.g., if self.auth is False, return 'noauth'.
        if value is False:
            return 'no' + axis_name

        if value is True:
            return axis_name

        return value

    def on_off(self, *args, **kwargs):
        assert not (args and kwargs)
        if args:
            axis_name, = args
            return 'on' if getattr(self, axis_name) else 'off'

        (axis_name, value), = kwargs.items()
        return 'on' if getattr(self, axis_name) == value else 'off'

    def to_dict(self):
        task = super(Task, self).to_dict()
        if self.tags:
            task['tags'] = self.tags
        task.update(self.options)
        if self.depends_on:
            task['depends_on'] = self.depends_on
        task['commands'] = self.commands
        return task


class NamedTask(Task):
    def __init__(self, task_name, commands=None, **kwargs):
        super(NamedTask, self).__init__(commands=commands, **kwargs)
        self._task_name = task_name

    @property
    def name(self):
        return self._task_name


class FuncTask(NamedTask):
    def __init__(self, task_name, *args, **kwargs):
        commands = [func(func_name) for func_name in args]
        super(FuncTask, self).__init__(task_name, commands=commands, **kwargs)


class Prohibited(Exception):
    pass


def require(rule):
    if not rule:
        raise Prohibited()


def prohibit(rule):
    if rule:
        raise Prohibited()


def both_or_neither(rule0, rule1):
    if rule0:
        require(rule1)
    else:
        prohibit(rule1)


class MatrixTask(Task):
    axes = OD()

    def __init__(self, *args, **kwargs):
        axis_dict = OD()
        for name, values in self.axes.items():
            # First value for each axis is the default value.
            axis_dict[name] = kwargs.pop(name, values[0])

        super(MatrixTask, self).__init__(*args, **kwargs)
        self.__dict__.update(axis_dict)

    @classmethod
    def matrix(cls):
        for cell in product(*cls.axes.values()):
            axis_values = dict(zip(cls.axes, cell))
            task = cls(**axis_values)
            if task.allowed:
                yield task

    @property
    def allowed(self):
        try:
            self._check_allowed()
            return True
        except Prohibited:
            return False

    def _check_allowed(self):
        pass