"""Print an Evergreen expansion file line with MONGODB_URI for the deployment started by dsc-start.sh.

Usage: dsc-uri.py <slslocal-state-dir>

Reads the newest ports.json under the state directory, then finds the primary with a minimal OP_MSG `hello`
(the host may not have mongosh) and connects to it directly.

Replica set discovery does not work from the host: the members advertise names like `shard1-node0-mongod`
that only resolve inside the containers (see the slslocal README), so the URI uses directConnection=true.
"""

import glob
import json
import os
import socket
import struct
import sys


def cstr(s):
    return s.encode() + b'\x00'


def hello_command():
    body = b'\x10' + cstr('hello') + struct.pack('<i', 1)
    body += b'\x10' + cstr('helloOk') + struct.pack('<i', 1)
    db = b'admin\x00'
    body += b'\x02' + cstr('$db') + struct.pack('<i', len(db)) + db
    doc = struct.pack('<i', len(body) + 5) + body + b'\x00'
    payload = b'\x00' + doc
    return struct.pack('<iiii', 16 + 4 + len(payload), 1, 0, 2013) + struct.pack('<I', 0) + payload


def recv_exact(sock, n):
    data = b''
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise RuntimeError('connection closed by server')
        data += chunk
    return data


def skip_value(etype, bson, pos):
    fixed = {0x01: 8, 0x07: 12, 0x08: 1, 0x09: 8, 0x10: 4, 0x11: 8, 0x12: 8, 0x13: 16, 0x0A: 0, 0xFF: 0, 0x7F: 0}
    if etype in fixed:
        return pos + fixed[etype]
    if etype in (0x02, 0x0D, 0x0E):
        return pos + 4 + struct.unpack('<i', bson[pos : pos + 4])[0]
    if etype in (0x03, 0x04, 0x0F):
        return pos + struct.unpack('<i', bson[pos : pos + 4])[0]
    if etype == 0x05:
        return pos + 5 + struct.unpack('<i', bson[pos : pos + 4])[0]
    if etype == 0x0B:
        p = bson.index(b'\x00', pos) + 1
        return bson.index(b'\x00', p) + 1
    raise RuntimeError(f'unhandled BSON element type {etype}')


def top_level_fields(bson):
    """Decode the top-level string and boolean fields of a BSON document."""
    out = {}
    size = struct.unpack('<i', bson[:4])[0]
    pos = 4
    while pos < size - 1:
        etype = bson[pos]
        pos += 1
        end = bson.index(b'\x00', pos)
        key = bson[pos:end].decode()
        pos = end + 1
        if etype == 0x02:
            n = struct.unpack('<i', bson[pos : pos + 4])[0]
            out[key] = bson[pos + 4 : pos + 4 + n - 1].decode()
        elif etype == 0x08:
            out[key] = bson[pos] != 0
        pos = skip_value(etype, bson, pos)
    return out


def hello(address):
    host, port = address.rsplit(':', 1)
    with socket.create_connection((host, int(port)), timeout=30) as sock:
        sock.settimeout(30)
        sock.sendall(hello_command())
        total = struct.unpack('<i', recv_exact(sock, 16)[:4])[0]
        body = recv_exact(sock, total - 16)
    return top_level_fields(body[5:])


def main():
    state_dir = sys.argv[1]
    paths = glob.glob(os.path.join(state_dir, '**', 'ports.json'), recursive=True)
    if not paths:
        sys.exit(f'no ports.json found under {state_dir}')
    with open(max(paths, key=os.path.getmtime)) as f:
        ports = json.load(f)
    shards = ports.get('shards_by_log_id') or {}
    hosts = [addr for nodes in shards.values() for addr in nodes.values()]
    if not hosts:
        sys.exit('ports.json has no mongod shards: ' + json.dumps(ports)[:500])

    for address in hosts:
        if hello(address).get('isWritablePrimary'):
            print(f'MONGODB_URI: "mongodb://{address}/?directConnection=true"')
            return
    sys.exit(f'no primary found among {hosts}')


main()
