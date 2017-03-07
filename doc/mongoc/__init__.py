from docutils.nodes import literal, Text
from docutils.parsers.rst import roles

from sphinx.roles import XRefRole


class SymbolRole(XRefRole):
    def __call__(self, *args, **kwargs):
        nodes, messages = XRefRole.__call__(self, *args, **kwargs)
        for node in nodes:
            attrs = node.attributes
            target = attrs['reftarget']
            parens = ''
            if target.endswith('()'):
                # Function call, :symbol:`mongoc_init()`
                target = target[:-2]
                parens = '()'

            if ':' in target:
                # E.g., 'bson:bson_t' has domain 'bson', target 'bson_t'
                attrs['domain'], name = target.split(':', 1)
                attrs['reftarget'] = name

                assert isinstance(node.children[0].children[0], Text)
                node.children[0].children[0] = Text(name + parens,
                                                    name + parens)

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
