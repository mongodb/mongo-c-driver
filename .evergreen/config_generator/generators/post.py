from shrub.v3.evg_project import EvgProject

from config_generator.etc import utils


def generate():
    post = []

    yaml = utils.to_yaml(EvgProject(post=post))
    utils.write_to_file(yaml, 'post.yml')
