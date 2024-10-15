from abc import ABC
import query_language.expressions as expressions
import query_language.node_matchers as node_matchers


class TraversalBase(ABC):
    def __init__(self, *args):
        self.name = f'_relation_{TraversalBase._get_next_relation_id()}'
        self.node = None
        self.args = []
        self.is_bound = False

        for arg in args:
            if isinstance(arg, node_matchers.NodeBase):
                if self.node is not None:
                    raise ValueError(
                        'Traversal matchers can only have one node matcher as argument!')
                self.node = arg
            else:
                self.args.append(arg)

    @staticmethod
    def _get_next_relation_id():
        try:
            TraversalBase._get_next_relation_id.counter += 1
        except AttributeError:
            TraversalBase._get_next_relation_id.counter = 0

        return TraversalBase._get_next_relation_id.counter

    def bind(self, name):
        self.name = name
        self.is_bound = True
        return self

    def emit(self, node_id):
        q = self.node.emit()
        rel = self.get_relation_template(self.name)
        q.match_list.append(f'({node_id}) {rel} ({self.node.name})')

        for arg in self.args:
            if isinstance(arg, expressions.ExprBase):
                q.where_list.append(arg.emit(self.name))
            else:
                raise ValueError('Expected expression matcher')

        if self.is_bound:
            q.bind_list.append(self.name)

        return q


class TraversalForwardRelation(TraversalBase):
    def get_relation_template(self, name):
        return f'-[{name}:{self.RELATION_TYPE}]->'


class TraversalBackwardRelation(TraversalBase):
    def get_relation_template(self, name):
        return f'<-[{name}:{self.RELATION_TYPE}]-'


class isInSameBasicBlock(TraversalBase):
    def get_relation_template(self, name):
        # NOTE: 'name' makes no sense here, because
        # there are two relations, so just ignore it.
        return '<-[:CONTAINS]-(:BasicBlock)-[:CONTAINS]->'


class annotates(TraversalForwardRelation):
    RELATION_TYPE = 'ANNOTATES'


class isAnnotatedBy(TraversalBackwardRelation):
    RELATION_TYPE = 'ANNOTATES'


class contains(TraversalForwardRelation):
    RELATION_TYPE = 'CONTAINS'


class occursIn(TraversalBackwardRelation):
    RELATION_TYPE = 'CONTAINS'


class dependsOn(TraversalForwardRelation):
    RELATION_TYPE = 'DEPENDS_ON'


class isDependencyOf(TraversalBackwardRelation):
    RELATION_TYPE = 'DEPENDS_ON'


class readsFrom(TraversalForwardRelation):
    RELATION_TYPE = 'READS'


class isReadFromBy(TraversalBackwardRelation):
    RELATION_TYPE = 'READS'


class writesTo(TraversalForwardRelation):
    RELATION_TYPE = 'WRITES'


class isWrittenToBy(TraversalBackwardRelation):
    RELATION_TYPE = 'WRITES'


class references(TraversalForwardRelation):
    RELATION_TYPE = 'REFERENCES'


class isReferencedBy(TraversalForwardRelation):
    RELATION_TYPE = 'REFERENCES'


class isInvokedBy(TraversalForwardRelation):
    RELATION_TYPE = 'INVOKED_BY'


class invokes(TraversalBackwardRelation):
    RELATION_TYPE = 'INVOKED_BY'
