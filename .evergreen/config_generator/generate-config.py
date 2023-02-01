#!/usr/bin/env python3

# Generates *.yml files under .evergreen/generated_configs.
#
# Install dependencies by running (preferably in a virtual environment):
#     python -m pip install -r .evergreen/config_generator/requirements.txt
#
# Invoke this using the command:
#     PYTHONPATH=".evergreen" python3 .evergreen/config_generator/generate-config.py


from sys import version_info
from importlib import import_module
from pathlib import Path


def all_generators():
    res = []

    # .evergreen/config_generator/generate_config.py -> .evergreen/config_generator/generators
    components_dir = Path(__file__).parent / 'generators'

    all_paths = components_dir.glob('**/*.py')

    for path in sorted(all_paths):
        generator_path = path.relative_to(components_dir)
        generator_str = str(generator_path.with_suffix(''))  # Drop '.py'.
        generator_str = generator_str.replace('/', '.')  # 'a/b' -> 'a.b'
        module_name = f'config_generator.generators.{generator_str}'
        res.append(import_module(module_name))

    return res


def main():
    # Requires Python 3.10 or newer.
    assert version_info.major >= 3
    assert version_info.minor >= 10

    for m in all_generators():
        MODULE_NAME = m.__name__.replace("config_generator.", "")
        print(f"Running {MODULE_NAME}.generate()...")
        m.generate()
        print(f"Running {MODULE_NAME}.generate()... done.")


if __name__ == '__main__':
    main()
