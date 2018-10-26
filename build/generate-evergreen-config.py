#!/usr/bin/env python
#
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

"""Generate C Driver's config.yml for Evergreen testing.

We find that generating configuration from Python data structures and a template
file is more legible than Evergreen's matrix syntax or a handwritten file.

Written for Python 2.6+, requires Jinja 2 for templating.
"""

import sys
from os.path import dirname, join as joinpath, normpath

try:
    # Python 3 abstract base classes.
    import collections.abc as abc
except ImportError:
    import collections as abc

try:
    import yaml
    import yamlordereddictloader
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    sys.stderr.write("try 'pip install -r build/requirements.txt'")
    raise


from evergreen_config_lib.tasks import all_tasks
from evergreen_config_lib.variants import all_variants

this_dir = dirname(__file__)
evergreen_dir = normpath(joinpath(this_dir, '../.evergreen'))


env = Environment(loader=FileSystemLoader(this_dir),
                  trim_blocks=True,
                  lstrip_blocks=True,
                  extensions=['jinja2.ext.loopcontrols'])

env.filters['tag_list'] = lambda value: (
        '[' + ', '.join('"%s"' % (tag,) for tag in value) + ']')

print('.evergreen/config.yml')
f = open(joinpath(evergreen_dir, 'config.yml'), 'w+')
t = env.get_template('config.yml.template')
f.write(t.render(all_tasks=all_tasks, all_variants=all_variants))
f.write('\n')
