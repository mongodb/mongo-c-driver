import subprocess


def generate():
    subprocess.run(
        args=[
            'python3',
            '.evergreen/legacy_config_generator/generate-evergreen-config.py',
        ],
        check=True,
    )
