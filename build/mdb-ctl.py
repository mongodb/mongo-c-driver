import argparse
from pathlib import Path
import sys
from typing import NamedTuple, Sequence, cast
import json
import shutil
import subprocess


class _CommandArgs(NamedTuple):
    mdb_exe: Path
    command: str


class _StartArgs(_CommandArgs):
    port: int
    server_args: Sequence[str]
    fixture_dir: Path


class _StopArgs(_CommandArgs):
    fixture_dir: Path


class _InitRSArgs(_CommandArgs):
    node_ports: Sequence[int]
    replset: str


def create_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '--mdb-exe',
        required=True,
        type=Path,
        help=
        'Path to a MongoD executable to use to run/control the server fixture',
        metavar='<path-to-mongod>')

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
    return parser


def _try_stop(dir: Path, mdb_exe: Path) -> bool:
    retc = subprocess.call([
        str(mdb_exe),
        f'--pidfilepath={dir}/mdb.pid',
        f'--dbpath={dir}/data',
        '--shutdown',
    ])
    return retc == 0


def _do_start(args: _StartArgs) -> int:
    if args.fixture_dir.is_dir():
        if not args.fixture_dir.joinpath('stopped.stamp').exists():
            _try_stop(args.fixture_dir, args.mdb_exe)
        shutil.rmtree(args.fixture_dir)
    args.fixture_dir.joinpath('data').mkdir(exist_ok=True, parents=True)
    subprocess.check_call([
        str(args.mdb_exe),
        f'--port={args.port}',
        f'--pidfilepath={args.fixture_dir}/mdb.pid',
        '--fork',
        '--verbose',
        f'--logpath={args.fixture_dir}/server.log',
        f'--dbpath={args.fixture_dir}/data',
        *args.server_args,
    ])
    return 0


def _do_stop(args: _StopArgs) -> int:
    if not _try_stop(args.fixture_dir, args.mdb_exe):
        raise RuntimeError(
            f'Failed to stop the MongoDB server in {args.fixture_dir}')
    return 0


def _do_init_rs(args: _InitRSArgs) -> int:
    mdb_bin_dir = args.mdb_exe.parent.resolve()
    msh_cands = (mdb_bin_dir.joinpath(f)
                 for f in ['mongo', 'mongosh', 'mongo.exe', 'mongosh.exe'])
    mongosh_exe = next(iter(filter(Path.is_file, msh_cands)), None)
    if mongosh_exe is None:
        raise RuntimeError(
            f'No MongoSH executable was found beside the MongoD executable')
    members_json = json.dumps([{
        '_id': idx,
        'host': f'localhost:{port}',
    } for idx, port in enumerate(args.node_ports)])
    init_js = rf'''
        var members = {members_json};
        rs.initiate({{
            _id: {repr(args.replset)},
            members: members,
        }});
        var i = 0;
        for (;;) {{
            if (rs.config().members.length == members.length)
                break;
            sleep(1);
            if (i == 10000) {{
                assert(false, 'Replica set members did not connect');
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


def main(argv: Sequence[str]) -> int:
    args = cast(_CommandArgs, create_argparser().parse_args(argv))
    if args.command == 'start':
        return _do_start(cast(_StartArgs, args))
    if args.command == 'stop':
        return _do_stop(cast(_StopArgs, args))
    if args.command == 'init-rs':
        return _do_init_rs(cast(_InitRSArgs, args))
    assert 0, f'Unknown command "{args.command}"'
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
