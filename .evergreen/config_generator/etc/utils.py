from importlib import import_module
from pathlib import Path
from textwrap import dedent

import yaml

from shrub.v3.evg_project import EvgProject
from shrub.v3.evg_task import EvgTask
from shrub.v3.evg_command import subprocess_exec


# Equivalent to EvgTask but with the run_on field.
class EvgTaskWithRunOn(EvgTask):
    run_on: str


# Automatically formats the provided script and invokes it in Bash.
def bash_exec(script, **kwargs):
    return subprocess_exec(
        binary='bash',
        args=['-c', dedent(script)],
        **kwargs
    )


def all_components():
    res = []

    # .evergreen/config_generator/etc/utils.py -> .evergreen/config_generator/components
    components_dir = Path(__file__).parent.parent / 'components'

    all_paths = components_dir.glob('**/*.py')

    for path in sorted(all_paths):
        component_path = path.relative_to(components_dir)
        component_str = str(component_path.with_suffix(''))  # Drop '.py'.
        component_str = component_str.replace('/', '.')  # 'a/b' -> 'a.b'
        module_name = f'config_generator.components.{component_str}'
        res.append(import_module(module_name))

    return res


# Helper function to print component name for diagnostic purposes.
def component_name(component):
    component_prefix = 'config_generator.components.'
    res = component.__name__[len(component_prefix):]
    return res


def write_to_file(yml, filename):
    # .evergreen/config_generator/etc/utils.py -> .evergreen
    evergreen_dir = Path(__file__).parent.parent.parent
    filename = evergreen_dir / 'generated_configs' / filename

    with open(filename.resolve(), 'w', encoding='utf-8') as file:
        file.write(yml)


class ConfigDumper(yaml.SafeDumper):
    # Represent multiline strings in the form:
    #     key: |
    #       multiline string
    #       multiline string
    #       multiline string
    def represent_scalar(self, tag, value, style=None):
        if isinstance(value, str) and '\n' in value:
            style = '|'

        return super().represent_scalar(tag, value, style)

    # Prefer using double quotes when able.
    def analyze_scalar(self, scalar):
        res = super().analyze_scalar(scalar)
        if res.allow_single_quoted and res.allow_double_quoted:
            res.allow_single_quoted = False
        return res

    # Represent flow mappings with space after left brace:
    #     node: { key: value }
    #            ^
    def expect_flow_mapping(self):
        super().expect_flow_mapping()
        self.write_indicator('', False)

    # Represent flow mappings with space before right brace:
    #     node: { key: value }
    #                       ^
    def expect_flow_mapping_key(self):
        if isinstance(self.event, yaml.MappingEndEvent):
            self.write_indicator(' ', False)
        super().expect_flow_mapping_key()

    # Allow for special-casing depending on parent node.
    def represent_special_mapping(self, tag, mapping, flow_style):
        value = []

        for item_key, item_value in mapping:
            node_key = self.represent_data(item_key)

            if item_key == 'tags':
                # Represent task tags using flow style to reduce line count:
                #     - name: task-name
                #       tags: [A, B, C]
                node_value = self.represent_sequence(
                    'tag:yaml.org,2002:seq', item_value, flow_style=True)
            elif item_key == 'depends_on' and len(item_value) == 1:
                # Represent task depends_on using flow style when only one
                # dependency is given to reduce line count:
                #     - name: task-name
                #       depends_on: [{ name: dependency }]
                node_value = self.represent_sequence(
                    'tag:yaml.org,2002:seq', item_value, flow_style=True)
            else:
                # Use default behavior.
                node_value = self.represent_data(item_value)

            value.append((node_key, node_value))

        node = yaml.MappingNode(tag, value, flow_style=flow_style)

        if self.alias_key is not None:
            self.represented_objects[self.alias_key] = node

        return node

    # Make an effort to order fields in a readable manner.
    # Ordering applies to *all* mappings regardless of the parent node.
    def represent_mapping(self, tag, mapping, flow_style=False):
        # Represent updates mapping for expansions.update commands using flow
        # style to reduce line count:
        #    - command: expansions.update
        #      params:
        #        updates:
        #          - { key: KEY, value: VALUE }
        #          - { key: KEY, value: VALUE }
        #          - { key: KEY, value: VALUE }
        if len(mapping) == 2 and 'key' in mapping and 'value' in mapping:
            flow_style = True

        before = [
            'name',
            'display_name',
            'command',
            'type',
            'run_on',
            'tags',
            'depends_on',
            'binary',
            'working_dir',
        ]

        after = [
            'commands',
            'args',
        ]

        ordered = {}  # Note: insertion-order preservation requires Python 3.7.
        suffix = {}

        for field_name in before:
            field = mapping.pop(field_name, None)
            if field:
                ordered[field_name] = field

        for field_name in after:
            field = mapping.pop(field_name, None)
            if field:
                suffix[field_name] = field

        ordered.update(sorted(mapping.items()))
        ordered.update(suffix)

        return self.represent_special_mapping(tag, ordered.items(), flow_style)

    # Ensure a block sequence is indented relative to its parent node::
    #     key:
    #       - a
    #       - b
    #       - c
    # instead of::
    #     key:
    #     - a
    #     - b
    #     - c
    def increase_indent(self, flow=None, indentless=None):
        indentless = False
        return super().increase_indent(flow=flow, indentless=indentless)


def to_yaml(project: EvgProject) -> str:
    return yaml.dump(
        project.dict(exclude_none=True, exclude_unset=True, by_alias=True),
        Dumper=ConfigDumper,
        default_flow_style=False,
        width=float('inf'),
    )
