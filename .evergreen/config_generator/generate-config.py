#!/usr/bin/env python3

# Generates *.yml files under .evergreen/generated_configs.
#
# Install dependencies by running (preferably in a virtual environment):
#     python -m pip install -r .evergreen/config_generator/requirements.txt
#
# Invoke this using the command:
#     PYTHONPATH=".evergreen" python3 .evergreen/config_generator/generate-config.py


from glob import iglob
from importlib import import_module
from pathlib import Path


def all_generators():
    res = []

    # .evergreen/config_generator/generate_config.py -> .evergreen/config_generator/generators
    components_dir = Path(__file__).parent / 'generators'

    all_paths = iglob(str(components_dir) + '/**/*.py', recursive=True)

    for path_str in sorted(all_paths):
        path = Path(path_str)
        if path == '__init__.py':
            continue

        if path.suffix != '.py':
            continue

        component_path = Path(path_str).relative_to(components_dir)
        component_path = str(component_path)[:-3]  # Drop '.py'.
        component_path = component_path.replace('/', '.')  # 'a/b' -> 'a.b'
        module_name = f'config_generator.generators.{component_path}'
        res.append(import_module(module_name))

    return res


def main():
    for m in all_generators():
        MODULE_NAME = m.__name__.replace("config_generator.", "")
        print(f"Running {MODULE_NAME}.generate()...")
        m.generate()
        print(f"Running {MODULE_NAME}.generate()... done.")


if __name__ == '__main__':
    main()
