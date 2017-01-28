from docutils.nodes import literal
from docutils.parsers.rst import roles

from sphinx.roles import XRefRole


class SymbolRole(XRefRole):
    def __call__(self, *args, **kwargs):
        nodes, messages = XRefRole.__call__(self, *args, **kwargs)
        for node in nodes:
            attrs = node.attributes
            target = attrs['reftarget']
            if target.endswith('()'):
                # Function call, :symbol:`mongoc_init()`
                target = target[:-2]

            if ':' in target:
                # E.g., 'bson:bson_t' has domain 'bson', target 'bson_t'
                attrs['domain'], attrs['reftarget'] = target.split(':', 1)
            else:
                attrs['reftarget'] = target

            attrs['reftype'] = 'doc'
            attrs['classes'].append('symbol')
        return nodes, messages


def setup(app):
    roles.register_local_role(
        'symbol', SymbolRole(warn_dangling=True, innernodeclass=literal))

    return {
        'version': '1.0',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
