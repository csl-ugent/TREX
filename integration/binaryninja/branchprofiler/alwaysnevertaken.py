from binaryninja import *
from mate_attack_framework.util import get_image_name, get_database

def integrate_always_never_taken_branches(binary_view: BinaryView):
    '''Marks branches that are always taken or never taken as such.'''
    db = get_database()
    cur_file = get_image_name(binary_view)

    always_taken_query = f'''
                          MATCH (ins:Instruction)
                          WHERE ins.num_branch_not_taken = 0
                          RETURN ins
                          '''

    never_taken_query = f'''
                         MATCH (ins:Instruction)
                         WHERE ins.num_branch_taken = 0
                         RETURN ins
                         '''

    binary_view.begin_undo_actions()

    for record in db.run_list(always_taken_query):
        addr = int(record['ins']['image_offset'])
        binary_view.always_branch(addr)

    for record in db.run_list(never_taken_query):
        addr = int(record['ins']['image_offset'])
        binary_view.never_branch(addr)

    binary_view.commit_undo_actions()
