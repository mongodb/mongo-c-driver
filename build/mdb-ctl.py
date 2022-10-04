import argparse
from datetime import timedelta, datetime
from pathlib import Path
import socket
import sys
import time
from typing import Literal, NamedTuple, Sequence, cast
import json
import shutil
import subprocess

import proc_ctl


class _CommandArgs(NamedTuple):
    """Common command argumetns"""
    mdb_exe: Path
    "The path to the mongod executable. Other program paths will be inferred from this."
    command: Literal['start', 'stop', 'init-rs', 'init-sharding',
                     'stop-sharding']
    "Which subcommand to run"


class _StartArgs(_CommandArgs):
    """Arguments for 'start'"""
    port: int
    "The port on which the server should listen"
    server_args: Sequence[str]
    "Additional command-line arguments to the server"
    fixture_dir: Path
    "The fixture directory for database data and control files"


class _StopArgs(_CommandArgs):
    """Arguments for 'stop'"""
    fixture_dir: Path
    "The fixture directory for database data and control files"


class _InitRSArgs(_CommandArgs):
    """Arguments for init-rs"""
    node_ports: Sequence[int]
    "The IP ports of all the nodes in the replica set"
    replset: str
    "The name of the replica set"


class _InitShardingArgs(_CommandArgs):
    """Arguments for init-sharding"""
    configdb: str
    "The --configdb for mongos"
    datadb: str
    "The shard to add to mongos"
    port: int
    "The IP port on which mongos should listen"
    fixture_dir: Path
    "The fixture directory for control files"


class _StopShardingArgs(_CommandArgs):
    """Arguments for stop-sharding"""
    fixture_dir: Path
    "The fixture directory for control files"


def create_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '--mdb-exe',
        required=True,
        type=Path,
        help=
        'Path to a MongoD executable to use to run/control the server fixture',
        metavar='<path-to-mongod-exe>')

    subs = parser.add_subparsers(required=True,
                                 title='Subcommand',
                                 help='The action to perform',
                                 dest='command')

    start = subs.add_parser('start')
    start.add_argument('--port',
                       type=int,
                       required=True,
                       help='The TCP port to which the server will bind')
    start.add_argument(
        '--server-args',
        nargs=argparse.REMAINDER,
        help='Additional arguments to pass to the server executable')
    start.add_argument('--fixture-dir',
                       required=True,
                       type=Path,
                       help='Directory to the fixture database data',
                       metavar='DIR')

    stop = subs.add_parser('stop')
    stop.add_argument('--fixture-dir',
                      required=True,
                      type=Path,
                      help='Directory to the fixture database data',
                      metavar='DIR')

    init_rs = subs.add_parser('init-rs')
    init_rs.add_argument(
        '--node-port',
        required=True,
        help='Port of a node in the replica set (Provide at least once)',
        metavar='PORT',
        action='append',
        dest='node_ports',
        type=int)
    init_rs.add_argument('--replset',
                         help='Name of the replica set',
                         required=True)

    init_sharding = subs.add_parser('init-sharding')
    init_sharding.add_argument(
        '--configdb',
        required=True,
        help='The specifier of the config db for mongos',
        metavar='<replset>/<host>')
    init_sharding.add_argument(
        '--datadb',
        required=True,
        help='The specifier of the data db to connect to the mongos',
        metavar='<replset>/<host>')
    init_sharding.add_argument('--port',
                               required=True,
                               help='The bind port for the mongos instance',
                               type=int)
    init_sharding.add_argument('--fixture-dir',
                               required=True,
                               help='Directory for ephemeral and pid files',
                               type=Path)

    stop_sharding = subs.add_parser('stop-sharding')
    stop_sharding.add_argument('--fixture-dir',
                               required=True,
                               help='Directory for ephemeral and pid files',
                               type=Path)
    return parser


def _wait_until_connectible(port: int,
                            *,
                            timeout: timedelta = timedelta(seconds=10)):
    """
    Attempt to connect to the given port over TCP on localhost.
    Spin until a connection succeeds

    :raise TimeoutError: If the timeout is reached before a connection is established.
    """
    expire = datetime.now() + timeout
    while 1:
        try:
            conn = socket.create_connection(('localhost', port))
        except ConnectionRefusedError:
            time.sleep(0.1)
        else:
            conn.close()
            break
        if expire < datetime.now():
            raise TimeoutError(
                f'Port {port} did not become connectible within the time limit'
            )


def _do_start(args: _StartArgs) -> int:
    """Implements the 'start' command"""
    try:
        # If there is anything already running here, stop it now.
        proc_ctl.ensure_not_running(args.fixture_dir)
    except RuntimeError as e:
        raise RuntimeError(
            f'Failed to stop already-running MongoDB server in [{args.fixture_dir}]'
        ) from e

    # Delete any prior data
    try:
        shutil.rmtree(args.fixture_dir)
    except FileNotFoundError:
        pass

    # Create the data directory
    args.fixture_dir.joinpath('data').mkdir(exist_ok=True, parents=True)

    # Spawn the server
    state = proc_ctl.start_process(
        ctl_dir=args.fixture_dir,
        cwd=args.fixture_dir,
        command=[
            str(args.mdb_exe),
            f'--port={args.port}',
            f'--pidfilepath={args.fixture_dir}/mdb.pid',
            '--verbose',
            f'--logpath={args.fixture_dir}/server.log',
            f'--dbpath={args.fixture_dir}/data',
            '--setParameter=shutdownTimeoutMillisForSignaledShutdown=500',
            *args.server_args,
        ])

    # If it returned non-none, the process is not running
    if not isinstance(state, int):
        if state['error']:
            raise RuntimeError(
                f'Failed to spawn MongoDB server process: {state["error"]}')
        raise RuntimeError(
            f'MongoDB server exited immediately [Exit {state["exit"]}]')

    try:
        _wait_until_connectible(args.port)
    except TimeoutError as e:
        proc_ctl.ensure_not_running(args.fixture_dir)
        raise RuntimeError('Failed to spawn MongoDB server')

    state = proc_ctl.get_pid_or_exit_result(args.fixture_dir)
    if not isinstance(state, int):
        raise RuntimeError(f'MongoDB server exited prematurely [{state}]')
    return 0


def _do_stop(args: _StopArgs) -> int:
    try:
        proc_ctl.ensure_not_running(args.fixture_dir)
    except RuntimeError as e:
        raise RuntimeError(
            f'Failed to stop the MongoDB server in {args.fixture_dir}') from e
    return 0


def _find_mongosh(bin_dir: Path) -> Path:
    msh_cands = (bin_dir.joinpath(f)
                 for f in ['mongo', 'mongosh', 'mongo.exe', 'mongosh.exe'])
    mongosh_exe = next(iter(filter(Path.is_file, msh_cands)), None)
    if mongosh_exe is None:
        raise RuntimeError(f'No MongoSH executable was found in {bin_dir}')
    return mongosh_exe


def _do_init_rs(args: _InitRSArgs) -> int:
    mongosh_exe = _find_mongosh(args.mdb_exe.parent)
    members_json = json.dumps([{
        '_id': idx,
        'host': f'localhost:{port}',
        'priority': 0 if idx != 0 else 1,
    } for idx, port in enumerate(args.node_ports)])
    init_js = rf'''
        var members = {members_json};
        rs.initiate({{
            _id: {repr(args.replset)},
            members: members,
        }});
        var i = 0;
        for (i = 0; rs.config().members.length != members.length; ++i) {{
            sleep(1);
            if (i == 20000) {{
                assert(false, 'Replica set members did not connect');
            }}
        }}
        for (i = 0; rs.status().members[0].state != 1; ++i) {{
            sleep (1);
            if (i == 20000) {{
                assert(false, 'First member did not become elected in time');
            }}
        }}
    '''
    subprocess.check_call([
        str(mongosh_exe),
        f'--port={args.node_ports[0]}',
        '--norc',
        '--eval',
        init_js,
    ])
    return 0


def _do_init_sharding(args: _InitShardingArgs) -> int:
    mongosh_exe = _find_mongosh(args.mdb_exe.parent)
    mdb_bin_dir = args.mdb_exe.parent.resolve()
    ms_cands = (mdb_bin_dir.joinpath(f) for f in ['mongos', 'mongos.exe'])
    mongos_exe = next(iter(ms_cands), None)
    if mongosh_exe is None:
        raise RuntimeError(
            f'No MongoS executable was found beside the MongoD executable')

    proc_ctl.ensure_not_running(args.fixture_dir)
    args.fixture_dir.mkdir(exist_ok=True, parents=True)
    proc_ctl.start_process(
        ctl_dir=args.fixture_dir,
        cwd=Path.cwd(),
        command=[
            str(mongos_exe),
            f'--configdb={args.configdb}',
            f'--port={args.port}',
            f'--logpath={args.fixture_dir}/mongos.log',
            f'--pidfilepath={args.fixture_dir}/mongos.pid',
            '--setParameter=mongosShutdownTimeoutMillisForSignaledShutdown=500',
            '--setParameter=enableTestCommands=1',
        ])

    # Starting up mongos can take some time. Spin wait until we can successfully
    # open a TCP connection to mongos.
    try:
        _wait_until_connectible(args.port)
    except TimeoutError as e:
        proc_ctl.ensure_not_running(args.fixture_dir)
        raise RuntimeError('Failed to spawn mongos') from e

    # Now add the datadb to our shards:
    subprocess.check_call([
        str(mongosh_exe),
        f'localhost:{args.port}/admin',
        '--norc',
        '--eval',
        f'sh.addShard({args.datadb!r})',
    ])

    return 0


def _do_stop_sharding(args: _StopShardingArgs) -> int:
    proc_ctl.ensure_not_running(args.fixture_dir)
    return 0


def main(argv: Sequence[str]) -> int:
    args = cast(_CommandArgs, create_argparser().parse_args(argv))
    if args.command == 'start':
        return _do_start(cast(_StartArgs, args))
    if args.command == 'stop':
        return _do_stop(cast(_StopArgs, args))
    if args.command == 'init-rs':
        return _do_init_rs(cast(_InitRSArgs, args))
    if args.command == 'init-sharding':
        return _do_init_sharding(cast(_InitShardingArgs, args))
    if args.command == 'stop-sharding':
        return _do_stop_sharding(cast(_StopShardingArgs, args))
    assert 0, f'Unknown command "{args.command}"'
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
