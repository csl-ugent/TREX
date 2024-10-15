from core.workspace import Workspace


class GDSGraph():
    def _is_normal_projection(self):
        return (self.nodes is not None) and (self.nodes != []) and \
               (self.edges is not None) and (self.edges != []) and \
               (self.node_query is None) and \
               (self.edge_query is None)

    def _is_cypher_projection(self):
        return (self.nodes is None) and \
               (self.edges is None) and \
               (self.node_query is not None) and \
               (self.edge_query is not None)

    def __init__(self, name, nodes=None, edges=None, node_query=None, edge_query=None):
        self.db = Workspace.current.graph
        self.name = name
        self.nodes = nodes
        self.edges = edges
        self.node_query = node_query
        self.edge_query = edge_query

        assert self._is_normal_projection() or self._is_cypher_projection()

    def __enter__(self):
        if self._is_normal_projection():
            node_list = ','.join([f"'{n}'" for n in self.nodes])
            edge_list = ','.join([f"'{e}'" for e in self.edges])

            self.db.run(f'''
                        CALL gds.graph.project('{self.name}', [{node_list}], [{edge_list}], {{ }})
                        ''')

        elif self._is_cypher_projection():
            self.db.run(f'''
                        CALL gds.graph.project.cypher('{self.name}', '{self.node_query}', '{self.edge_query}', {{ }})
                        ''')

        return self

    def __exit__(self, exception_type, exception_value, exception_traceback):
        self.db.run(f'''
                     CALL gds.graph.drop('{self.name}')
                     ''')

    def num_nodes(self):
        return self.db.run_evaluate(f'''CALL gds.graph.list('{self.name}') YIELD nodeCount''')

    def num_edges(self):
        return self.db.run_evaluate(f'''CALL gds.graph.list('{self.name}') YIELD relationshipCount''')

    def num_weakly_connected_components(self):
        return self.db.run_evaluate(f'''CALL gds.wcc.stats('{self.name}') YIELD componentCount''')

    def cyclomatic_complexity(self):
        return self.num_edges() - self.num_nodes() + self.num_weakly_connected_components()

    def dijkstra_source_target(self, source_id=None, target_id=None, node_labels=['*'], relationship_types=['*']):
        assert source_id is not None
        assert target_id is not None

        return self.db.run_to_df(f'''
                            CALL gds.shortestPath.dijkstra.stream('{self.name}', {{
                                sourceNode: {source_id},
                                targetNode: {target_id},
                                nodeLabels: {node_labels},
                                relationshipTypes: {relationship_types}
                            }})
                            ''')

    def dijkstra_single_source(self, source_id=None, node_labels=['*'], relationship_types=['*']):
        assert source_id is not None

        return self.db.run_to_df(f'''
                            CALL gds.allShortestPaths.dijkstra.stream('{self.name}', {{
                                sourceNode: {source_id},
                                nodeLabels: {node_labels},
                                relationshipTypes: {relationship_types}
                            }})
                            ''')
