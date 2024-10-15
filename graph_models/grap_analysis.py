from graph_models.analysis import Analysis

# BROKEN


class GrapAnalysis(Analysis):
    __primarylabel__ = 'Grap'
    Static = Label()
    PatternMatcher = Label()

    patterns = RelatedTo('PatternOccurrence', 'MATCHED_PATTERN')

    def __init__(self):
        super(GrapAnalysis, self).__init__()
        self.Static = True
        self.PatternMatcher = True


class PatternOccurrence(Model):
    pattern = Property()
    start_address = Property()
    end_address = Property()

    nodes = RelatedTo(Model, 'HAS_NODE')
    analysis = RelatedFrom('PatternMatcher', 'MATCHED_PATTERN')
