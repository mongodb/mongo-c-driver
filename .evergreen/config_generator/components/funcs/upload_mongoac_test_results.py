from shrub.v3.evg_command import attach_xunit_results

from config_generator.etc.function import Function
from config_generator.etc.utils import bash_exec


class UploadMongoacTestResults(Function):
    name = 'upload-mongoac-test-results'
    commands = [
        # Ensure attach_xunit_results does not fail even if no test results exist.
        bash_exec(
            script="""\
                mkdir -p mongoc
                touch mongoc/mongoac-test-results.xml
            """
        ),
        attach_xunit_results(file='mongoc/mongoac-test-results.xml'),
    ]


def functions():
    return UploadMongoacTestResults.defn()
