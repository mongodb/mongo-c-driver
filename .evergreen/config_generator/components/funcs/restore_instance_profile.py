from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class RestoreInstanceProfile(Function):
    name = 'restore-instance-profile'
    commands = [
        bash_exec(
            script='''\
            # Restore the AWS Instance Profile that may have been removed in AWS tasks.

            if [[ ! -d drivers-evergreen-tools ]]; then
                echo "drivers-evergreen-tools not present ... skipping"
                exit 0
            fi

            cd "drivers-evergreen-tools/.evergreen/auth_aws"
            if [ -f "./aws_e2e_setup.json" ]; then
                . ./activate-authawsvenv.sh
                python ./lib/aws_assign_instance_profile.py
            fi
            '''
        ),
    ]

    @classmethod
    def call(cls, **kwargs):
        return cls.default_call(**kwargs)


def functions():
    return RestoreInstanceProfile.defn()
