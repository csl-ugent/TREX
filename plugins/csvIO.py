import os
from typing import AnyStr, Dict

from core.workspace import Workspace
from neo4j_py2neo_bridge import Graph


class CsvIO:
    @staticmethod
    def csv_insert_disassembly(image_name: AnyStr, disassembly):
        files = CsvIO.disassembly_to_csv(disassembly, prefix=disassembly.module)
        CsvIO.import_disassembly_CSV(image_name, files, '')

    @staticmethod
    def import_disassembly_CSV(
            image_name: AnyStr, csv_files: Dict[AnyStr, AnyStr], csv_files_folder: AnyStr):
        # canonicalize image_name
        image_name = os.path.basename(image_name)

        def execute_queries(db: Graph) -> None:
            cleanup_query = []
            # Functions ---------------------------------------------------------
            if 'functions' in csv_files:
                print("Inserting functions..")
                # insert function nodes
                query = """
                                LOAD CSV WITH HEADERS FROM "file:///functions" AS line
                                CREATE (f: Function {id: toInteger(line.id), name: line.name})
                                """
                db.run(query)
                constraint_name = db.create_constraint_unique('Function', 'id')
                db.create_index('Function', ['name'])

                cleanup_query.append(f"DROP CONSTRAINT {constraint_name}")
                cleanup_query.append("MATCH (n) WHERE n:Function REMOVE n.id")

            # Blocks ------------------------------------------------------------
            print("Inserting basic blocks..")
            ftn_rel = ""
            if 'functions' in csv_files:
                ftn_rel = """
                                WITH new_block, line
                                MATCH (function:Function { id: toInteger(line.function_id) })
                                CREATE (function)-[:HAS_BLOCK]->(new_block)
                                """
            query = f"""
                            CALL {{
                                LOAD CSV WITH HEADERS FROM 'file:///blocks' AS line
                                CREATE (new_block: BasicBlock {{id: toInteger(line.id),
                                                            image_name: "{image_name}",
                                                            image_offset_begin: toInteger(line.startEA),
                                                            image_offset_end: toInteger(line.endEA)
                                                            }})
                                {ftn_rel}
                            }} IN TRANSACTIONS OF 1000 ROWS
                            """

            db.run(query)
            db.create_index("BasicBlock", ["image_name", "image_offset_begin", "image_offset_end"])
            constraint_name = db.create_constraint_unique('BasicBlock', 'id')

            cleanup_query.append(f"DROP CONSTRAINT {constraint_name}")
            cleanup_query.append("MATCH (n) WHERE n:BasicBlock REMOVE n.id")

            # Instructions ------------------------------------------------------
            print("Inserting instructions..")
            query = f"""
                            CALL {{
                                LOAD CSV WITH HEADERS FROM 'file:///instructions' AS line
                                CREATE (new_ins: Instruction {{image_name: "{image_name}",
                                                            image_offset: toInteger(line.address),
                                                            assembly: line.assembly,
                                                            mnem: line.mnem,
                                                            operands: split(line.operands,';')
                                                            }}) WITH new_ins, line
                                MATCH (basic_block:BasicBlock {{ id: toInteger(line.block_id)}})
                                CREATE (basic_block)-[:HAS_INSTRUCTION {{last: toBoolean(line.last)}}]->(new_ins)
                            }} IN TRANSACTIONS OF 1000 ROWS
                            """
            db.run(query)

            db.create_index("Instruction", ["image_name", "image_offset"])
            db.create_index("Instruction", ["image_name"])
            db.create_index("Instruction", ["image_offset"])

            # Edges -------------------------------------------------------------
            print("Inserting edges..")
            query = f"""
                    CALL {{
                        LOAD CSV WITH HEADERS FROM 'file:///edges' AS line
                        MATCH (src_block:BasicBlock {{ id: toInteger(line.block_id)}}) WITH src_block, line
                        MATCH (i:Instruction {{ image_name: "{image_name}", image_offset: toInteger(line.dest_address)}})<-[:HAS_INSTRUCTION]-(dest_block:BasicBlock)
                        CREATE (src_block)-[:HAS_CF_EDGE {{type: line.edge_type}}]->(dest_block)
                    }} IN TRANSACTIONS OF 1000 ROWS
                    """
            db.run(query)

            # Calls -------------------------------------------------------------
            if 'calls' in csv_files:
                print("Inserting calls..")
                query = f"""
                                CALL {{
                                    LOAD CSV WITH HEADERS FROM 'file:///calls' AS line
                                    WITH line WHERE line.dest_name IS NOT NULL
                                    MATCH (src_inst:Instruction {{ image_name: "{image_name}", image_offset: toInteger(line.ins_address) }})
                                    MERGE (f:Function {{ name: line.dest_name}})
                                    MERGE (fsrc:Function {{ name: line.ins_function_name }})
                                    SET f.external = toBoolean(line.plt)
                                    CREATE (src_inst)-[:HAS_CALL_TARGET]->(f)
                                    CREATE (fsrc)-[:HAS_CALL_TARGET]->(f)
                                }} IN TRANSACTIONS OF 1000 ROWS
                                """
                db.run(query)

            # Strings
            if 'strings' in csv_files:
                print('Inserting strings...')
                query = f'''
                         CALL {{
                            LOAD CSV WITH HEADERS FROM 'file:///strings' AS line
                            CREATE (str:String {{ content: line.string, type: line.strtype, length: line.length, address: toInteger(line.address) }})
                         }} IN TRANSACTIONS OF 1000 ROWS
                         '''

                db.run(query)

            if 'stringxrefs' in csv_files:
                print('Inserting string xrefs...')
                query = f'''
                         CALL {{
                            LOAD CSV WITH HEADERS FROM 'file:///stringxrefs' AS line
                            MATCH (i:Instruction {{ image_name: "{image_name}", image_offset: toInteger(line.xref_addr) }})
                            MATCH (s:String {{ address: toInteger(line.string_address) }})
                            CREATE (i) -[:REFERENCES]-> (s)
                         }} IN TRANSACTIONS OF 1000 ROWS
                         '''

                db.run(query)

            print("Running cleanup queries")
            for sub_query in cleanup_query:
                db.run(sub_query)

        Workspace.current.import_csv_files(csv_files_folder, csv_files, execute_queries)

    @staticmethod
    def disassembly_to_csv(dis, prefix=None):
        block_file_path = "{}_blocks.csv".format(prefix or dis.__primaryvalue__)
        ins_file_path = "{}_ins.csv".format(prefix or dis.__primaryvalue__)
        edge_file_path = "{}_edges.csv".format(prefix or dis.__primaryvalue__)
        with open(block_file_path, mode='w') as block_file,\
             open(ins_file_path, mode='w') as ins_file,\
             open(edge_file_path, mode='w') as edge_file:
            # write headers
            block_file.write("id,startEA,endEA,function_id\n")
            ins_file.write("address,block_id,last,assembly,mnem,operands\n")
            edge_file.write("id,block_id,ins_address,dest_address,plt,edge_type\n")

            block_id = -1
            for block in dis.basic_blocks:
                block_id += 1
                block_file.write(','.join([str(block_id), str(block.image_offset_begin), str(block.image_offset_end), '']) + '\n')
                for ins in block.instructions:
                    ins_file.write(','.join([str(ins.image_offset), str(block_id), '', '', ins.mnem, ';'.join(ins.operands)]) + '\n')
                for edge in block.out_set._related_objects:
                    dst_block = edge[0]
                    edge_type = edge[1]['type']
                    edge_file.write(','.join(['', str(block_id), '', str(dst_block.image_offset_begin), '', str(edge_type)]) + '\n')
        return {'blocks': block_file_path, 'instructions': ins_file_path, 'edges': edge_file_path}
