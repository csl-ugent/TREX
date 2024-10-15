import inspect
import os
import shutil

from core.core import Core
from core.workspace import Workspace
from modules.ida import IDADism
from modules.metrics import GDSGraph
from modules.strings import find_strings_by_keywords, get_references_of_string

from query_language.expressions import *
from query_language.node_matchers import *
from query_language.traversal_matchers import *

from query_language.get_query import get_query


def demo():
    for bin in ['binary1', 'binary2']:
        filename = inspect.getframeinfo(inspect.currentframe()).filename
        path = os.path.dirname(os.path.abspath(filename))
        demofile = os.path.join(path, f'{bin}-stripped')
        Workspace.create_new(f'experiment-2024-06-{bin}', demofile)
        Workspace.select(f'experiment-2024-06-{bin}')
        db = Workspace.current.graph

        # Run IDA disassembly.
        ida = IDADism()
        ida.run()

        # GDS graph of entire program.
        print('GDS graph of entire program:')
        with GDSGraph('complexity', nodes=['BasicBlock'], edges=['HAS_CF_EDGE']) as graph:
            print(f'Number of nodes: {graph.num_nodes()}')
            print(f'Number of edges: {graph.num_edges()}')
            print(f'Number of WCCs:  {graph.num_weakly_connected_components()}')
            print(f'Cyclomatic complexity:  {graph.cyclomatic_complexity()}')
        print()

        # GDS graph of main function.
        print('GDS graph of the main function:')
        with GDSGraph('complexity',
                      node_query='MATCH (fun:Function) -[:HAS_BLOCK]-> (bbl:BasicBlock) WHERE fun.name = "main" RETURN id(bbl) AS id',
                      edge_query='MATCH (fun:Function) -[:HAS_BLOCK]-> (src:BasicBlock) -[:HAS_CF_EDGE]-> (target:BasicBlock) WHERE fun.name = "main" RETURN id(src) as source, id(target) as target') as graph:
            print(f'Number of nodes: {graph.num_nodes()}')
            print(f'Number of edges: {graph.num_edges()}')
            print(f'Number of WCCs:  {graph.num_weakly_connected_components()}')
            print(f'Cyclomatic complexity:  {graph.cyclomatic_complexity()}')
        print()

        # String module.
        keywords = {
            r'(?i)license': +10,
            r'(?i)registration': +10,
            r'(?i)key': +10,
            r'(?i)wrong': +5,
            r'(?i)incorrect': +5,
        }

        for (s, priority) in find_strings_by_keywords(keywords):
            if priority == 0:
                continue
            content = s['str']['content']
            print(priority, content if len(content) < 80 else content[0:79] + '...')

            for ref in get_references_of_string(s['str']['address']):
                ins = ref['ins']
                print('\t', f"{ins['image_name']}+{hex(ins['image_offset'])}: {ins['mnem']} {', '.join(ins['operands'])}")
            print()

    Workspace.select(f'experiment-2024-06-binary1')
    db = Workspace.current.graph

    # Distance module.
    with GDSGraph('distance', nodes=['BasicBlock'], edges=['HAS_CF_EDGE']) as graph:
        src = db.run_evaluate(get_query(
            BasicBlock(
                (P("image_name") == 'binary1-stripped') & (P("image_offset_begin") == 0xee89) & (P("image_offset_end") == 0xeeb2)
            ).bind("src")
        ))

        dst = db.run_evaluate(get_query(
            BasicBlock(
                (P("image_name") == 'binary1-stripped') & (P("image_offset_begin") == 0xefd4) & (P("image_offset_end") == 0xf006)
            ).bind("dst")
        ))

        print(f'Dijkstra shortest path between {src} and {dst}:')
        res1 = graph.dijkstra_source_target(
            source_id=src.id,
            target_id=dst.id,
            node_labels=['BasicBlock'],
            relationship_types=['HAS_CF_EDGE'],
        )
        print(res1)
        print()

        print(f'Dijkstra single source all paths from {src}:')
        res2 = graph.dijkstra_single_source(
            source_id=src.id,
            node_labels=['BasicBlock'],
            relationship_types=['HAS_CF_EDGE'],
        )
        print(res2)
        print()

        print(f'Nodes closer to {src} than to {dst}: ', end='')
        for node in set(res1.targetNode):
            dst1 = res1[res1.targetNode == node].iloc[0].totalCost
            dst2 = float('inf')

            try:
                dst2 = res2[res2.targetNode == node].iloc[0].totalCost
            except:
                pass

            if dst1 < dst2:
                print(node, end=', ')
        print()
