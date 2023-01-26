from shrub.v3.evg_command import EvgCommandType
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class PrepareKerberos:
    @classmethod
    def name(cls):
        return 'prepare-kerberos'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                command_type=EvgCommandType.SETUP,
                working_dir='mongoc',
                silent=True,
                script='''\
                if test "${keytab|}" && [[ -f /etc/krb5.conf ]]; then
                    echo "${keytab}" > /tmp/drivers.keytab.base64
                    base64 --decode /tmp/drivers.keytab.base64 > /tmp/drivers.keytab
                    if touch /etc/krb5.conf 2>/dev/null; then
                        cat .evergreen/etc/kerberos.realm | tee -a /etc/krb5.conf
                    elif command sudo true 2>/dev/null; then
                        cat .evergreen/etc/kerberos.realm | sudo tee -a /etc/krb5.conf
                    else
                        echo "Cannot append kerberos.realm to /etc/krb5.conf; skipping." 1>&2
                    fi
                fi
                '''
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls):
        return FunctionCall(func=cls.name())


def functions():
    res = {}

    res.update(PrepareKerberos.defn())

    return res
