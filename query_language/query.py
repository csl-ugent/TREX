from itertools import combinations


class Query:
    def __init__(self, match_list: list = None,
                 where_list: list = None,
                 bind_list: list = None):
        self.match_list = match_list if match_list is not None else []
        self.where_list = where_list if where_list is not None else []
        self.bind_list = bind_list if bind_list is not None else []

    def __add__(self, other):
        return Query(
            self.match_list + other.match_list,
            self.where_list + other.where_list,
            self.bind_list + other.bind_list)

    def to_string(self, distinct_binds=False):
        match_clause = ', '.join(self.match_list)

        if not self.where_list:
            where_clause = 'true'
        else:
            where_clause = ' AND '.join(
                [f'({part})' for part in self.where_list])

        if not self.bind_list:
            raise ValueError('Must bind at least one value!')
        else:
            bind_clause = ', '.join(self.bind_list)

        # Ensure that each bound node is different
        if not distinct_binds:
            distinct_clause = 'true'
        else:
            distinct_clause = ' AND '.join(
                [f'({i[0]} <> {i[1]})'
                 for i in combinations(self.bind_list, 2)
                 ]
            )

        return f'''MATCH {match_clause}
                   WHERE ({where_clause})
                   AND   ({distinct_clause})
                   RETURN {bind_clause}'''
