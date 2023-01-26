from shrub.v3.evg_project import EvgProject

from config_generator.components.funcs.fetch_source import FetchSource
from config_generator.etc import utils


def generate():
    pre = []

    pre.append(FetchSource.call())

    yaml = utils.to_yaml(EvgProject(pre=pre))
    utils.write_to_file(yaml, 'pre.yml')
