from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class StopLoadBalancer:
    @classmethod
    def name(cls):
        return 'stop-load-balancer'

    @classmethod
    def defn(cls):
        commands = []

        commands.append(
            bash_exec(
                script='''\
                    # Only run if a load balancer was started.
                    if [[ -z "${SINGLE_MONGOS_LB_URI}" ]]; then
                        echo "OK - no load balancer running"
                        exit
                    fi
                    if [[ -d drivers-evergreen-tools ]]; then
                        cd drivers-evergreen-tools && .evergreen/run-load-balancer.sh stop
                    fi
                '''
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(StopLoadBalancer.defn())

    return res
