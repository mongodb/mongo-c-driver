import subprocess
from pathlib import Path
import sys

HERE = Path(__file__).parent.resolve()
EVG_DIR = HERE.parent.parent


def generate():
    subprocess.run(
        args=[
            sys.executable,
            str(EVG_DIR / "legacy_config_generator/generate-evergreen-config.py"),
        ],
        check=True,
    )
