from __future__ import annotations

import sys
import time
import json
import traceback
import functools
import bottle
from bottle import Bottle, HTTPResponse, request

imds = Bottle(autojson=True)
"""An Azure IMDS server"""

from typing import TYPE_CHECKING, Any, Callable, Iterable, overload

if TYPE_CHECKING:
    from typing import Protocol

    class _RequstParams(Protocol):

        def __getitem__(self, key: str) -> str:
            ...

        @overload
        def get(self, key: str) -> str | None:
            ...

        @overload
        def get(self, key: str, default: str) -> str:
            ...

    class _HeadersDict(dict[str, str]):

        def raw(self, key: str) -> bytes | None:
            ...

    class _Request(Protocol):
        query: _RequstParams
        params: _RequstParams
        headers: _HeadersDict

    request: _Request


def parse_qs(qs: str) -> dict[str, str]:
    return dict(bottle._parse_qsl(qs))  # type: ignore


def require(cond: bool, message: str):
    if not cond:
        print(f'REQUIREMENT FAILED: {message}')
        raise bottle.HTTPError(400, message)


_HandlerFuncT = Callable[
    [],
    'None|str|bytes|dict[str, Any]|bottle.BaseResponse|Iterable[bytes|str]']


def handle_asserts(fn: _HandlerFuncT) -> _HandlerFuncT:

    @functools.wraps(fn)
    def wrapped():
        try:
            return fn()
        except AssertionError as e:
            traceback.format_exc()
            print(e.args)
            return bottle.HTTPResponse(status=400,
                                       body=json.dumps({'error':
                                                        list(e.args)}))

    return wrapped


def test_flags() -> dict[str, str]:
    return parse_qs(request.headers.get('X-MongoDB-HTTP-TestParams', ''))


def maybe_pause():
    pause = int(test_flags().get('pause', '0'))
    if pause:
        print(f'Pausing for {pause} seconds')
        time.sleep(pause)


@imds.get('/metadata/identity/oauth2/token')
@handle_asserts
def get_oauth2_token():
    api_version = request.query['api-version']
    assert api_version == '2018-02-01', 'Only api-version=2018-02-01 is supported'
    resource = request.query['resource']
    assert resource == 'https://vault.azure.net', 'Only https://vault.azure.net is supported'

    flags = test_flags()
    maybe_pause()

    case = flags.get('case')
    print('Case is:', case)
    if case == '404':
        return HTTPResponse(status=404)

    if case == '500':
        return HTTPResponse(status=500)

    if case == 'bad-json':
        return b'{"key": }'

    if case == 'empty-json':
        return b'{}'

    if case == 'giant':
        return _gen_giant()

    if case == 'slow':
        return _slow()

    assert case is None or case == '', f'Unknown HTTP test case "{case}"'

    return {
        'access_token': 'magic-cookie',
        'expires_in': '60',
        'token_type': 'Bearer',
        'resource': 'https://vault.azure.net',
    }


def _gen_giant() -> Iterable[bytes]:
    yield b'{ "item": ['
    for _ in range(1024 * 256):
        yield (b'null, null, null, null, null, null, null, null, null, null, '
               b'null, null, null, null, null, null, null, null, null, null, '
               b'null, null, null, null, null, null, null, null, null, null, '
               b'null, null, null, null, null, null, null, null, null, null, ')
    yield b' null ] }'
    yield b'\n'


def _slow() -> Iterable[bytes]:
    yield b'{ "item": ['
    for _ in range(1000):
        yield b'null, '
        time.sleep(1)
    yield b' null ] }'


if __name__ == '__main__':
    print(f'RECOMMENDED: Run this script using bottle.py in the same '
          f'directory (e.g. [{sys.executable} bottle.py fake_azure:imds])')
    imds.run()
