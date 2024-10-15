from binaryninja import *
from mate_attack_framework.util import get_image_name, get_database, get_function_containing_address, little_endian_byte_str_to_num

def integrate_value_set_analysis(binary_view: BinaryView):
    '''Adds the values read by instructions to Binary Ninja's Value Set Analysis.'''
    db = get_database()
    cur_file = get_image_name(binary_view)

    query = f'''
        MATCH (ins:Instruction)
        WHERE ins.image_name CONTAINS "{cur_file}"
        AND ins.read_values IS NOT NULL
        AND ins.written_values IS NOT NULL
        RETURN ins'''

    for record in db.run_list(query):
        addr = int(record['ins']['image_offset'])
        read_vals = record['ins']['read_values']
        written_vals = record['ins']['written_values']

        # Set comment.
        comment = binary_view.get_comment_at(addr)

        if read_vals != '[]':
            comment += f'read values:    {read_vals}\n'
        if written_vals != '[]':
            comment += f'written values: {written_vals}\n'

        binary_view.set_comment_at(addr, comment)

        # Integrate with BinaryNinja's VSA.
        # e.g. read_vals = '[00 00 00 00, 01 00 00 00]'
        #      read_vals_arr = [0, 1]
        read_vals_stripped = read_vals.strip('[]')
        read_vals_arr = []

        # split of empty string still returns one value, so only split for non-empty strings
        if read_vals_stripped != '':
            read_vals_arr = [little_endian_byte_str_to_num(i) for i in read_vals_stripped.split(',')]

        # only update VSA when there are fewer than 5 results, i.e. we know that only these
        # values are possible
        if len(read_vals_arr) < 5:
            try:
                if len(read_vals_arr) == 1:
                    value_set = PossibleValueSet.constant(read_vals_arr[0])
                else:
                    value_set = PossibleValueSet.in_set_of_values(read_vals_arr)
                function = get_function_containing_address(binary_view, addr)

                mlil = function.get_low_level_il_at(addr).medium_level_il

                # we only consider MLIL of the form '<var> = [<memory>]'
                # we need to check three conditions for this:
                # (1) the instruction is an MLIL_SET_VAR
                # (2) the instruction's LHS is a variable
                # (3) the instruction's RHS is an MLIL_LOAD

                if (mlil.operation == MediumLevelILOperation.MLIL_SET_VAR) and \
                   (type(mlil.operands[0]) == Variable) and \
                   (type(mlil.operands[1]) == MediumLevelILLoad) and \
                   (mlil.operands[1].operation == MediumLevelILOperation.MLIL_LOAD):
                    # destination variable for VSA is the instruction's LHS
                    var = mlil.operands[0]
                    function.set_user_var_value(var, mlil.address, value_set)
            except Exception as e:
                log.log(log.LogLevel.WarningLog, f'Ignoring MLIL instruction: {e}')
