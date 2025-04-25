from shrub.v3.evg_command import EvgCommandType

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class CleanupAuth(Function):
    name = 'cleanup-auth'
    commands = [
        bash_exec(
            command_type=EvgCommandType.SETUP,
            working_dir='mongoc',
            script='''\
            # Remove temporary files created for auth testing.
            test -e /tmp/atlas_x509.pem && rm /tmp/atlas_x509.pem
            test -e /tmp/atlas_x509_pkcs1.pem && rm /tmp/atlas_x509_pkcs1.pem
            test -e /tmp/drivers.keytab.base64 && rm /tmp/drivers.keytab.base64
            test -e /tmp/drivers.keytab && rm /tmp/drivers.keytab
            '''
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return CleanupAuth.defn()
