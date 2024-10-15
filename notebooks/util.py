# ====================================================
# Utility functions that can be used in all notebooks.
# ====================================================

from neo4j_py2neo_bridge import Graph
import pandas


def get_nodes_of_type(db: Graph, node_type: str) -> pandas.DataFrame:
    '''
    Create a Pandas DataFrame with all nodes of a given type.
    Each row represents a different node, and each column represents a
    property of the node.

    Parameters:
    - db (py2neo.database.Graph): The database that the nodes will be obtained
      from.
    - node_type (str): The label of the node to obtain (e.g. Instruction,
      BasicBlock, etc.)

    Returns:
    - pandas.DataFrame: A Pandas DataFrame containing the nodes.
    '''

    # First, get a list of all properties of that node type.
    properties = [entry['k'] for entry in
                  db.run_list(f'''
                         MATCH (node:{node_type})
                         UNWIND keys(node) AS k
                         RETURN DISTINCT k
                         ''')]

    # Create a query that returns each property of the node
    return_clause = ','.join([f'node.{prop} AS {prop}' for prop in properties])
    query = f'''
             MATCH (node:{node_type})
             RETURN {return_clause}
             '''

    # Execute that query, and convert the result to a DataFrame.
    return db.run_to_df(query)


def unpack_dataframe(df):
    ret = pandas.DataFrame()

    for col in df.columns:
        properties = set()

        for i, row in df[col].iteritems():
            properties |= set(row.keys())

        for prop in properties:
            ret[f'{col}.{prop}'] = df.apply(lambda entry: entry[col][prop], axis = 1)

            # Avoid "PerformanceWarning: DataFrame is highly fragmented." error.
            ret = ret.copy()

    return ret
