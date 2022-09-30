"""
Extremely basic subprocess control
"""

import argparse
import json
import os
import random
import signal
import subprocess
import sys
import time
import traceback
from datetime import datetime, timedelta
from pathlib import Path
from typing import TYPE_CHECKING, NoReturn, Sequence, Union, cast, NewType

if TYPE_CHECKING:
    from typing import (Literal, NamedTuple, TypedDict)

INTERUPT_SIGNAL = signal.SIGINT if os.name != 'nt' else signal.CTRL_C_SIGNAL

PID = NewType('PID', int)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser('proc_ctl.py')
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
                       nargs='+',
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
    ll_run.add_argument('child_command', nargs='+')

    return parser


if TYPE_CHECKING:
    StartCommandArgs = NamedTuple('StartCommandArgs', [
        ('command', Literal['start']),
        ('ctl_dir', Path),
        ('cwd', Path),
        ('child_command', Sequence[str]),
        ('spawn_wait', float),
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

    ChildExitResult = TypedDict('_ResultType', {
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
        write_text(self.pid_file, str(pid))

    def _get_pid(self) -> 'PID | None':
        try:
            txt = self.pid_file.read_text()
        except FileNotFoundError:
            return None
        return PID(int(txt))

    def set_exit(self, exit: 'str | int | None', error: 'str | None') -> None:
        write_text(self.result_file, json.dumps({
            'exit': exit,
            'error': error
        }))
        remove_file(self.pid_file)

    def state(self) -> 'None | ChildExitResult | PID':
        return self._get_pid() or self._get_result()

    def _get_result(self) -> 'None | ChildExitResult':
        try:
            txt = self.result_file.read_text()
        except FileNotFoundError:
            return None
        return json.loads(txt)

    def clear_result(self) -> None:
        remove_file(self.result_file)

    def signal_stop(self) -> None:
        pid = self.state()
        if not isinstance(pid, int):
            raise ProcessLookupError
        os.kill(pid, INTERUPT_SIGNAL)


def wait_stopped(
    ctl_dir: Path, *,
    timeout: timedelta = timedelta(seconds=5)) -> 'ChildExitResult':
    child = _ChildControl(ctl_dir)
    """
    Wait for a child process to exit.

    :raise ProcessLookupError: If there is no running process or exit result
        associated with the given directory.
    :raise TimeoutError: If the child is still running after the timeout expires.
    """
    # Spin around until the state is no longer a PID
    expire = datetime.now() + (timeout or timedelta())
    state = child.state()
    while isinstance(state, int):
        if expire < datetime.now():
            raise TimeoutError(f'Process [{state}] is still running')
        time.sleep(0.05)
        state = child.state()

    if state is None:
        # There was never a child here
        raise ProcessLookupError
    return state


def get_pid_or_exit_result(ctl_dir: Path) -> 'None | ChildExitResult | PID':
    """
    Get the state of a child process for the given control directory.

    :returns PID: If there is a child running in this directory.
    :returns ChildExitResult: If there was a child spawned for this directory,
        but has since exited.
    :returns None: If there is no child running nor a record of one having
        existed.
    """
    return _ChildControl(ctl_dir).state()


def ensure_not_running(
    ctl_dir: Path, *,
    timeout: timedelta = timedelta(seconds=5)) -> 'None | ChildExitResult':
    """
    Ensure no child is running for the given control directory.

    If a child is found, it will be asked to stop, and we will wait 'timeout' for it to exit

    :return None: If there was never a child running in this directory.
    :return ChildExitResult: The exit result of the child process, if it had
        once been running.
    :raise TimeoutError: If the process does not exit after the given timeout.
    """
    child = _ChildControl(ctl_dir)
    try:
        child.signal_stop()
    except ProcessLookupError:
        # The process is not running, or was never started
        r = child.state()
        assert not isinstance(r, int), (r, ctl_dir, timeout)
        return r
    return wait_stopped(ctl_dir, timeout=timeout)


class ChildStartupError(RuntimeError):
    pass


def start_process(*, ctl_dir: Path, cwd: Path,
                  command: Sequence[str]) -> 'PID | ChildExitResult':
    """
    Spawn a child process with a result recorder.

    The result of the spawn can later be observed using
    :func:`get_pid_or_exit_result` called with the same ``ctl_dir``.
    """
    ll_run_cmd = [
        sys.executable,
        '-u',
        '--',
        __file__,
        '__run',
        '--ctl-dir={}'.format(ctl_dir),
        '--',
        *command,
    ]
    ctl_dir.mkdir(exist_ok=True, parents=True)
    child = _ChildControl(ctl_dir)
    state = child.state()
    if isinstance(state, int):
        raise RuntimeError(
            'Child process is already running [PID {}]'.format(state))
    child.clear_result()
    assert child.state() is None

    # Spawn the child controller process
    subprocess.Popen(ll_run_cmd,
                     cwd=cwd,
                     stderr=subprocess.STDOUT,
                     stdout=ctl_dir.joinpath('runner-output.txt').open('wb'),
                     stdin=subprocess.DEVNULL)

    # Wait for a PID or exit result to appear
    expire = datetime.now() + timedelta(seconds=5)
    state = child.state()
    while state is None:
        if expire < datetime.now():
            raise TimeoutError(f'Process [{command}] is not starting')
        time.sleep(0.05)
        state = child.state()

    # Check that it actually spawned
    if not isinstance(state, int) and state['error']:
        raise ChildStartupError(
            f'Error while spawning child process: {state["error"]}')
    return state


def _start(args: 'StartCommandArgs') -> int:
    expire = datetime.now() + timedelta(seconds=args.spawn_wait)
    state = start_process(ctl_dir=args.ctl_dir,
                          cwd=args.cwd,
                          command=args.child_command)
    while not isinstance(state, dict) and expire > datetime.now():
        time.sleep(0.05)
    if not isinstance(state, int):
        # The child process exited or failed to spawn
        if state['error']:
            print(state['error'], file=sys.stderr)
        raise ChildStartupError('Child exited prematurely [Exited {}]'.format(
            state['exit']))
    return 0


def _stop(args: 'StopCommandArgs') -> int:
    child = _ChildControl(args.ctl_dir)
    try:
        child.signal_stop()
    except ProcessLookupError:
        if args.if_not_running == 'fail':
            raise RuntimeError('Child process is not running')
        elif args.if_not_running == 'ignore':
            # Nothing to do
            return 0
        else:
            assert False
    result = wait_stopped(args.ctl_dir,
                          timeout=timedelta(seconds=args.stop_wait))
    if result is None:
        raise RuntimeError(
            'Child process did not exit within the grace period')
    return 0


def __run(args: '_RunCommandArgs') -> int:
    this = _ChildControl(args.ctl_dir)
    try:
        pipe = subprocess.Popen(
            args.child_command,
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


def write_text(fpath: Path, content: str):
    """
    "Atomically" write a new file.

    This writes the given ``content`` into a temporary file, then renames that
    file into place. This prevents readers from seeing a partial read.
    """
    tmp = fpath.with_name(fpath.name + '.tmp')
    remove_file(tmp)
    tmp.write_text(content)
    os.sync()
    remove_file(fpath)
    tmp.rename(fpath)


def remove_file(fpath: Path):
    """
    Safely remove a file.

    Because Win32, deletes are asynchronous, so we rename to a random filename,
    then delete that file. This ensures the file is "out of the way", even if
    it takes some time to delete.
    """
    delname = fpath.with_name(fpath.name + '.delete-' +
                              str(random.randint(0, 999999)))
    try:
        fpath.rename(delname)
    except FileNotFoundError:
        return
    delname.unlink()


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
