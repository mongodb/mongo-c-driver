"""
Extremely basic subprocess control
"""

import argparse
import json
import os
import signal
import subprocess
import sys
import traceback
from datetime import datetime, timedelta
from pathlib import Path
from typing import TYPE_CHECKING, NoReturn, Sequence, Union, cast

if TYPE_CHECKING:
    from typing import (Literal, NamedTuple, TypedDict)

INTERUPT_SIGNAL = signal.SIGINT if os.name != 'nt' else signal.CTRL_C_SIGNAL


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser('proc-ctl')
    grp = parser.add_subparsers(title='Commands',
                                dest='command',
                                metavar='<subcommand>')

    start = grp.add_parser('start', help='Start a new subprocess')
    start.add_argument('--ctl-dir',
                       help='The control directory for the subprocess',
                       required=True,
                       type=Path)
    start.add_argument('--cwd',
                       help='The new subdirectory of the spawned process',
                       type=Path)
    start.add_argument(
        '--spawn-wait',
        help='Number of seconds to wait for child to be running',
        type=float,
        default=3)
    start.add_argument('child_command',
                       nargs=argparse.REMAINDER,
                       help='The command to execute',
                       metavar='<command> [args...]')

    stop = grp.add_parser('stop', help='Stop a running subprocess')
    stop.add_argument('--ctl-dir',
                      help='The control directory for the subprocess',
                      required=True,
                      type=Path)
    stop.add_argument('--stop-wait',
                      help='Number of seconds to wait for stopping',
                      type=float,
                      default=5)
    stop.add_argument('--if-not-running',
                      help='Action to take if the child is not running',
                      choices=['fail', 'ignore'],
                      default='fail')

    ll_run = grp.add_parser('__run')
    ll_run.add_argument('--ctl-dir', type=Path, required=True)
    ll_run.add_argument('child_command', nargs=argparse.REMAINDER)

    return parser


if TYPE_CHECKING:
    StartCommandArgs = NamedTuple('StartCommandArgs', [
        ('command', Literal['start']),
        ('ctl_dir', Path),
        ('cwd', Path),
        ('child_command', Sequence[str]),
        ('spawn_wait', int),
    ])

    StopCommandArgs = NamedTuple('StopCommandArgs', [
        ('command', Literal['stop']),
        ('ctl_dir', Path),
        ('stop_wait', float),
        ('if_not_running', Literal['fail', 'ignore']),
    ])

    _RunCommandArgs = NamedTuple('_RunCommandArgs', [
        ('command', Literal['__run']),
        ('child_command', Sequence[str]),
        ('ctl_dir', Path),
    ])

    CommandArgs = Union[StartCommandArgs, StopCommandArgs, _RunCommandArgs]

    _ResultType = TypedDict('_ResultType', {
        'exit': 'str | int | None',
        'error': 'str | None'
    })


def parse_argv(argv: 'Sequence[str]') -> 'CommandArgs':
    parser = create_parser()
    args = parser.parse_args(argv)
    return cast('CommandArgs', args)


class _ChildControl:

    def __init__(self, ctl_dir: Path) -> None:
        self._ctl_dir = ctl_dir

    @property
    def pid_file(self):
        """The file containing the child PID"""
        return self._ctl_dir / 'pid.txt'

    @property
    def result_file(self):
        """The file containing the exit result"""
        return self._ctl_dir / 'exit.json'

    def set_pid(self, pid: int):
        self.pid_file.write_text(str(pid))

    def get_pid(self) -> 'int | None':
        try:
            txt = self.pid_file.read_text()
        except FileNotFoundError:
            return None
        return int(txt)

    def set_exit(self, exit: 'str | int | None', error: 'str | None') -> None:
        self.result_file.write_text(json.dumps({'exit': exit, 'error': error}))
        self._unlink(self.pid_file)

    def get_result(self) -> 'None | _ResultType':
        try:
            txt = self.result_file.read_text()
        except FileNotFoundError:
            return None
        try:
            return json.loads(txt)
        except json.JSONDecodeError:
            return self.get_result()

    def clear_result(self) -> None:
        self._unlink(self.result_file)

    @staticmethod
    def _unlink(p: Path) -> None:
        try:
            p.unlink()
        except FileNotFoundError:
            pass


def _start(args: 'StartCommandArgs') -> int:
    ll_run_cmd = [
        sys.executable,
        '-u',
        '--',
        __file__,
        '__run',
        '--ctl-dir={}'.format(args.ctl_dir),
        '--',
        *args.child_command[1:],
    ]
    args.ctl_dir.mkdir(exist_ok=True, parents=True)
    child = _ChildControl(args.ctl_dir)
    if child.get_pid() is not None:
        raise RuntimeError('Child process is already running [PID {}]'.format(
            child.get_pid()))
    child.clear_result()
    # Spawn the child controller
    subprocess.Popen(
        ll_run_cmd,
        cwd=args.cwd,
        stderr=subprocess.STDOUT,
        stdout=args.ctl_dir.joinpath('.runner-output.txt').open('wb'),
        stdin=subprocess.DEVNULL)
    expire = datetime.now() + timedelta(seconds=args.spawn_wait)
    # Wait for the PID to appear
    while child.get_pid() is None and child.get_result() is None:
        if expire < datetime.now():
            break
    # Check that it actually spawned
    if child.get_pid() is None:
        result = child.get_result()
        if result is None:
            raise RuntimeError('Failed to spawn child runner?')
        if result['error']:
            print(result['error'], file=sys.stderr)
        raise RuntimeError('Child exited immediately [Exited {}]'.format(
            result['exit']))
    # Wait to see that it is still running after --spawn-wait seconds
    while child.get_result() is None:
        if expire < datetime.now():
            break
    # A final check to see if it is running
    result = child.get_result()
    if result is not None:
        if result['error']:
            print(result['error'], file=sys.stderr)
        raise RuntimeError('Child exited prematurely [Exited {}]'.format(
            result['exit']))
    return 0


def _stop(args: 'StopCommandArgs') -> int:
    child = _ChildControl(args.ctl_dir)
    pid = child.get_pid()
    if pid is None:
        if args.if_not_running == 'fail':
            raise RuntimeError('Child process is not running')
        elif args.if_not_running == 'ignore':
            # Nothing to do
            return 0
        else:
            assert False
    os.kill(pid, INTERUPT_SIGNAL)
    expire_at = datetime.now() + timedelta(seconds=args.stop_wait)
    while expire_at > datetime.now() and child.get_result() is None:
        pass
    result = child.get_result()
    if result is None:
        raise RuntimeError(
            'Child process did not exit within the grace period')
    return 0


def __run(args: '_RunCommandArgs') -> int:
    this = _ChildControl(args.ctl_dir)
    try:
        pipe = subprocess.Popen(
            args.child_command[1:],
            stdout=args.ctl_dir.joinpath('child-output.txt').open('wb'),
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL)
    except:
        this.set_exit('spawn-failed', traceback.format_exc())
        raise
    this.set_pid(pipe.pid)
    retc = None
    try:
        while 1:
            try:
                retc = pipe.wait(0.5)
            except subprocess.TimeoutExpired:
                pass
            except KeyboardInterrupt:
                pipe.send_signal(INTERUPT_SIGNAL)
            if retc is not None:
                break
    finally:
        this.set_exit(retc, None)
    return 0


def main(argv: 'Sequence[str]') -> int:
    args = parse_argv(argv)
    if args.command == 'start':
        return _start(args)
    if args.command == '__run':
        return __run(args)
    if args.command == 'stop':
        return _stop(args)
    return 0


def start_main() -> NoReturn:
    sys.exit(main(sys.argv[1:]))


if __name__ == '__main__':
    start_main()
