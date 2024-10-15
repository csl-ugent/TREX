from query_language.expressions import _to_expr_node
from query_language.node_matchers import NodeBase


def _emit_sorter(sorter):
    try:
        return ', '.join([_to_expr_node(s).emit(None) + ' DESCENDING'
                          for s in sorter])
    except TypeError:
        return _to_expr_node(sorter).emit(None) + ' DESCENDING'


def get_query(matcher, sorter=None, distinct_binds=False):
    # Reset node counter to ensure reproducible names.
    NodeBase._reset_next_node_id()

    if sorter is not None:
        order_by_clause = ' ORDER BY ' + _emit_sorter(sorter)
    else:
        order_by_clause = ''

    query = matcher.emit().to_string(distinct_binds) + order_by_clause

    return query
