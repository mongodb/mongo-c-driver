from shrub.v3.evg_command import attach_results
from shrub.v3.evg_command import FunctionCall

from config_generator.etc.utils import bash_exec


class UploadTestResults:
    @classmethod
    def name(cls):
        return 'upload-test-results'

    @classmethod
    def defn(cls):
        commands = []

        # Ensure attach_results does not fail even if no tests results exist.
        commands.append(
            bash_exec(
                script='''\
                    mkdir mongoc
                    touch mongoc/test-results.json
                '''
            )
        )

        commands.append(
            attach_results(
                file_location='mongoc/test-results.json'
            )
        )

        return {cls.name(): commands}

    @classmethod
    def call(cls, **kwargs):
        return FunctionCall(func=cls.name(), **kwargs)


def functions():
    res = {}

    res.update(UploadTestResults.defn())

    return res
