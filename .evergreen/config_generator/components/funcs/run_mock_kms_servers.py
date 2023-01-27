from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class RunMockKMSServers:
    @classmethod
    def name(cls):
        return 'run-mock-kms-servers'

    @classmethod
    def defn(cls):
        command_type = EvgCommandType.SETUP

        commands = []

        # This command ensures future invocations of activate-kmstlsvenv.sh conducted in
        # parallel do not race to setup a venv environment; it has already been prepared.
        # This primarily addresses the situation where the "run tests" and "run-mock-kms-servers"
        # functions invoke 'activate-kmstlsvenv.sh' simultaneously.
        # TODO: remove this function along with the "run-mock-kms-servers" function.
        commands.append(
            bash_exec(
                command_type=command_type,
                working_dir='drivers-evergreen-tools/.evergreen/csfle',
                add_expansions_to_env=True,
                script='''\
                    set -o errexit
                    echo "Preparing KMS TLS venv environment..."
                    # TODO: remove this function along with the "run kms servers" function.
                    if [[ "$OSTYPE" =~ cygwin && ! -d kmstlsvenv ]]; then
                        # Avoid using Python 3.10 on Windows due to incompatible cipher suites.
                        # See CDRIVER-4530.
                        . ../venv-utils.sh
                        venvcreate "C:/python/Python39/python.exe" kmstlsvenv || # windows-2017
                        venvcreate "C:/python/Python38/python.exe" kmstlsvenv    # windows-2015
                        python -m pip install --upgrade boto3~=1.19 pykmip~=0.10.0
                        deactivate
                    else
                        . ./activate-kmstlsvenv.sh
                        deactivate
                    fi
                    echo "Preparing KMS TLS venv environment... done."
                ''',
            )
        )

        commands.append(
            bash_exec(
                command_type=command_type,
                background=True,
                working_dir='drivers-evergreen-tools/.evergreen/csfle',
                script='''\
                    set -o errexit
                    echo "Starting mock KMS TLS servers..."
                    . ./activate-kmstlsvenv.sh
                    python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --port 8999 &
                    python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/expired.pem --port 9000 &
                    python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/wrong-host.pem --port 9001 &
                    python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --require_client_cert --port 9002 &
                    python -u kms_kmip_server.py &
                    deactivate
                    echo "Starting mock KMS TLS servers... done."
                ''',
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(RunMockKMSServers.defn())

    return res
