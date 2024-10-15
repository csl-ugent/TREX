import re

from core.workspace import Workspace

from query_language.expressions import *
from query_language.node_matchers import *
from query_language.traversal_matchers import *

from query_language.get_query import get_query


def get_string_priority(s, keywords):
    total_score = 0

    for keyword, score in keywords.items():
        if re.search(keyword, s):
            total_score += score

    return total_score


def find_strings_by_keywords(keywords):
    db = Workspace.current.graph

    all_strings = db.run_list(get_query(
        String().bind('str')
        ))

    all_strings_with_priority = sorted([(s, get_string_priority(s['str']['content'], keywords)) for s in all_strings], key=lambda x: -x[1])

    return all_strings_with_priority


def get_references_of_string(string_address):
    db = Workspace.current.graph

    return db.run_list(get_query(
        Instruction(
            references(
                String(P('address') == string_address)
                )
            ).bind('ins')
        ))
