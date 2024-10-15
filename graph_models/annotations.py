# BROKEN

ANNOTATES = Relationship.type("ANNOTATES")


@property
def annotations(self):
    annotator = False
    annotatable = False
    try:
        annotator = self._annotator
        annotatable = self._annotatable
    except AttributeError:
        pass
    return NodeAnnotations(self.__node__, annotatable, annotator)


def annotatable(cls):
    setattr(cls, 'annotations', annotations)

    def annotated_by(self, target_node, **properties):
        return ANNOTATES(target_node, self.__node__, **properties)
    setattr(cls, 'annotated_by', annotated_by)
    setattr(cls, '_annotatable', True)
    return cls


def annotator(cls):
    setattr(cls, 'annotations', annotations)

    def annotates(self, target_node, **properties):
        if isinstance(target_node, Model):
            target_node = target_node.__node__
        return ANNOTATES(self.__node__, target_node, **properties)

    setattr(cls, 'annotates', annotates)
    setattr(cls, '_annotator', True)
    return cls


class NodeAnnotations:
    def __init__(self, node, annotatable, annotator):
        self.node = node
        self.annotatable = annotatable
        self.annotator = annotator

    @property
    def incoming(self):
        if not self.annotatable:
            raise AttributeError('incoming')
        return RelationshipMatcher(self.node.graph).match([None, self.node], 'ANNOTATES')

    @property
    def outgoing(self):
        if not self.annotator:
            raise AttributeError('outgoing')
        return RelationshipMatcher(self.node.graph).match([self.node, None], 'ANNOTATES')
