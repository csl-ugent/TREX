from abc import ABC
from query_language.query import Query
from query_language.expressions import ExprBase
from query_language.traversal_matchers import TraversalBase


class NodeBase(ABC):
    def __init__(self, *args):
        self.args = list(args)
        self.name = f'_node_{NodeBase._get_next_node_id()}'
        self.is_bound = False

    @staticmethod
    def _reset_next_node_id():
        NodeBase._get_next_node_id.counter = 0

    @staticmethod
    def _get_next_node_id():
        try:
            NodeBase._get_next_node_id.counter += 1
        except AttributeError:
            NodeBase._get_next_node_id.counter = 0

        return NodeBase._get_next_node_id.counter

    def bind(self, name):
        self.name = name
        self.is_bound = True
        return self

    def emit(self) -> Query:
        q = Query()

        for arg in self.args:
            if isinstance(arg, ExprBase):
                q.where_list.append(arg.emit(self.name))
            elif isinstance(arg, TraversalBase):
                q += arg.emit(self.name)
            else:
                raise ValueError(f'Expected expression or traversal matcher, but {arg} was neither.')

        if self.NODE_TYPE is not None:
            q.match_list.append(f'({self.name}:{self.NODE_TYPE})')
        else:
            q.match_list.append(f'({self.name})')

        if self.is_bound:
            q.bind_list.append(self.name)

        return q


class ArbitraryNode(NodeBase):
    NODE_TYPE = None


class BasicBlock(NodeBase):
    NODE_TYPE = 'BasicBlock'


class Caballero(NodeBase):
    NODE_TYPE = 'Caballero'


class Function(NodeBase):
    NODE_TYPE = 'Function'


class Grap(NodeBase):
    NODE_TYPE = 'Grap'


class Instruction(NodeBase):
    NODE_TYPE = 'Instruction'


class MemoryBuffer(NodeBase):
    NODE_TYPE = 'MemoryBuffer'


class MemoryRegion(NodeBase):
    NODE_TYPE = 'MemoryRegion'


class SystemCall(NodeBase):
    NODE_TYPE = 'SystemCall'


class String(NodeBase):
    NODE_TYPE = 'String'
