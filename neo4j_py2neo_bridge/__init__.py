import time
from typing import List, Optional, Tuple

from neo4j import GraphDatabase

Neo4JCredentials = Tuple[str, str]

neo4j_default_credentials: Neo4JCredentials = ('neo4j', 'test')


class GraphService:
    def __init__(self, uri: str, auth: Optional[Neo4JCredentials] = neo4j_default_credentials):
        self.driver = GraphDatabase.driver(uri, auth=auth)
        self.driver.verify_connectivity()

    def __iter__(self):
        with self.driver.session() as session:
            query_res = session.run('SHOW DATABASES YIELD name')
            database_names = [item['name'] for item in query_res]
            return iter(database_names)

    def __getitem__(self, graph_name):
        if graph_name is None:
            raise ValueError('graph_name should not be None!')

        for graph_name_i in self:
            if graph_name_i == graph_name:
                return Graph(graph_name_i, self.driver)

        raise ValueError(f'Graph {graph_name} does not exist!')

    @property
    def system_graph(self):
        return self['system']


class Graph:
    def __init__(self, name, driver):
        self.name = name
        self.driver = driver

    def get_session(self):
        return self.driver.session(database=self.name)

    def run(self, cypher):
        with self.get_session() as session:
            session.run(cypher)

    def run_list(self, cypher):
        with self.get_session() as session:
            results = session.run(cypher)
            return list(results)

    def run_to_df(self, cypher):
        with self.get_session() as session:
            results = session.run(cypher)
            return results.to_df()

    def run_evaluate(self, cypher):
        with self.get_session() as session:
            results = session.run(cypher)

            # Obtain the first row of the result.
            value = results.single(strict=True)

            # Return the first column of the result.
            return value[0]

    def run_debug_json(self, cypher):
        with self.get_session() as session:
            results = session.run(cypher)
            return results.data()

    def create_constraint_unique(self, label: str, prop: str) -> str:
        name = f"{label}_{prop}_uniq"
        self.run(f"CREATE CONSTRAINT {name} IF NOT EXISTS FOR (l:{label}) REQUIRE l.{prop} IS UNIQUE")
        self.await_indices()
        return name

    def await_indices(self):
        start = time.perf_counter()
        self.run('call db.awaitIndexes')
        end = time.perf_counter()
        print(f"Building indices took {end - start}s")

    def create_index(self, label: str, props: List[str]):
        idx_props = ", ".join([f'l.{prop}' for prop in props])
        self.run(f'CREATE INDEX IF NOT EXISTS FOR (l:{label}) ON ({idx_props})')
        self.await_indices()
