import os
import math
from typing import Optional
import time

from .applications import Application

from core.core import Core
from core.workspace import Workspace

from modules.basic_block_profiler import BasicBlockProfilerModule
from modules.caballero import Caballero as CaballeroModule
from modules.data_dependencies import DataDependenciesModule
from modules.instruction_info import InstructionInfoModule
from modules.instruction_values import InstructionValuesModule
from modules.lldb import lldb
from modules.memory_instructions_profiler import MemoryInstructionsProfilerModule
from modules.metrics import GDSGraph
from modules.syscall_trace import SysCallTraceModule

from notebooks import util

from query_language.expressions import *
from query_language.node_matchers import *
from query_language.traversal_matchers import *
from query_language.get_query import get_query

import numpy as np
import scipy.stats
from itertools import chain, combinations
import shutil
import pandas as pd
from collections import defaultdict
import sklearn.cluster
import re
from sklearn.utils._testing import ignore_warnings
from sklearn.exceptions import ConvergenceWarning

class CryptoLocalisationTechnique:
    '''
    Class for functionality specific to the localisation technique.
    '''

    # The application to use
    app = None

    # The current timing ID
    current_timing_id = None

    # The current timing timestamp
    current_timing_timestamp = None

    # Timing file
    current_timing_file = None

    def __init__(self, application: Application) -> None:
        '''
        Creates a new instantiation of the localisation technique.

        Parameters:
        - application (Application): The application to perform the technique on.
        '''
        self.app = application
        app_name = self.app.__class__.__name__
        self.current_timing_file = open(f'/tmp/timing-{app_name}.csv', 'w')
        self.current_timing_file.write('timing_id,runtime_seconds\n')
        self.current_timing_file.flush()

    def start_timing(self, timing_id):
        if self.current_timing_id is not None:
            raise RuntimeError('Cannot start multiple timings at once!')

        self.current_timing_id = timing_id
        self.current_timing_timestamp = time.perf_counter()

    def stop_timing(self, timing_id):
        if self.current_timing_id != timing_id:
            raise RuntimeError('Mismatched timing!')

        total = time.perf_counter() - self.current_timing_timestamp
        self.current_timing_file.write(f'"{self.current_timing_id}",{total}\n')
        self.current_timing_file.flush()

        self.current_timing_id = None
        self.current_timing_timestamp = None


    #######################
    #  COLLECTING TRACES  #
    #######################
    def collect_traces(self, force_record=False) -> dict:
        sizes = self.app.get_step_1_input_sizes()

        self.start_timing('collect_traces')

        # Create input files of different sizes
        for size in sizes:
            input_path = os.path.join(self.app.workspace.path, f'input_{size}')
            self.app.create_input_file(input_path, size)

        traces = {}

        # Run the application with encryption, for different inputs
        for size in sizes:
            self.app.remove_output_file()
            binary_params = self.app.get_binary_params(f'input_{size}')
            recorder = Workspace.current.create_recorder(binary_params, pinball_suffix=f'input_{size}', force_record=force_record)
            traces[f'input_{size}'] = recorder
            recorder.run()

        # Run the application once more, with the smallest input but with encryption disabled
        self.app.remove_output_file()
        binary_params = self.app.get_binary_params(f'input_{sizes[0]}', perform_encryption = False)
        recorder = Workspace.current.create_recorder(binary_params, pinball_suffix=f'input_{sizes[0]}_noenc', force_record=force_record)
        traces[f'input_{sizes[0]}_noenc'] = recorder
        recorder.run()

        # Run the application with the largest input, but with another passphrase.
        self.app.remove_output_file()
        binary_params = self.app.get_binary_params(f'input_{max(sizes)}', passphrase = 'somethingelse')
        recorder = Workspace.current.create_recorder(binary_params, pinball_suffix=f'input_{max(sizes)}_altpassphrase', force_record=force_record)
        traces[f'input_{max(sizes)}_altpassphrase'] = recorder
        recorder.run()

        self.stop_timing('collect_traces')

        return traces

    ############
    #  STEP 1  #
    ############

    def step_1_localise_crypto_bbls_collect_data(self, traces) -> None:
        '''
        Collects the data for the first step of the technique: finding the crypto basic blocks.
        '''
        sizes = self.app.get_step_1_input_sizes()

        # (1) Caballero
        self.start_timing('step1_collectdata_caballero')
        caballero = CaballeroModule(ratio=0.15, binary_params='', timeout=0, recorder=traces[f'input_{sizes[0]}'])
        caballero.run()
        self.stop_timing('step1_collectdata_caballero')

        for size in sizes:
            input_filename = f'input_{size}'

            # (2) Basic block profiler
            self.start_timing(f'step1_collectdata_bblprofiler_input{size}')
            bbl_profiler = BasicBlockProfilerModule(binary_params='', timeout=0, properties_prefix=input_filename + '_', recorder=traces[input_filename])
            bbl_profiler.run()
            self.stop_timing(f'step1_collectdata_bblprofiler_input{size}')

            # (3) Memory instructions profiler
            self.start_timing(f'step1_collectdata_meminstprofiler_input{size}')
            mem_ins_profiler = MemoryInstructionsProfilerModule(binary_params='', timeout=0, properties_prefix=input_filename + '_', recorder=traces[input_filename], instruction_values_limit = 10000)
            mem_ins_profiler.run()
            self.stop_timing(f'step1_collectdata_meminstprofiler_input{size}')

        # (3) Memory instructions profiler
        # Run memory instructions profiler once more, with the largest input and a different passphrase.
        size = max(sizes)
        input_filename = f'input_{size}'
        self.start_timing(f'step1_collectdata_meminstprofiler_input{size}_altpassphrase')
        mem_ins_profiler = MemoryInstructionsProfilerModule(binary_params='', timeout=0, properties_prefix='altpassphrase_' + input_filename + '_', recorder=traces[input_filename + '_altpassphrase'], instruction_values_limit = 10000)
        mem_ins_profiler.run()
        self.stop_timing(f'step1_collectdata_meminstprofiler_input{size}_altpassphrase')

        # run basic block profiler once more, but without enabling encryption this time
        input_filename = f'input_{sizes[0]}'

        self.start_timing('step1_collectdata_bblprofiler_noenc')
        bbl_profiler = BasicBlockProfilerModule(binary_params='', timeout=0, properties_prefix=input_filename + '_noenc_', recorder=traces[input_filename + '_noenc'])
        bbl_profiler.run()
        self.stop_timing('step1_collectdata_bblprofiler_noenc')

        # (4) Instruction info
        self.start_timing('step1_collectdata_instructioninfo')
        instr_info = InstructionInfoModule(binary_params='', timeout=0, recorder=traces[f'input_{sizes[0]}'])
        instr_info.run()
        self.stop_timing('step1_collectdata_instructioninfo')

    def step_1_localise_crypto_bbls_analyse_data(self, groundtruth_basic_blocks: list, ignore_priority_components: list = [[]]) -> None:
        '''
        Performs the analysis for the first step of the technique: finding the crypto basic blocks.

        Parameters:
        - groundtruth_basic_blocks (list): A list of basic blocks that are related to crypto (i.e. the
          groundtruth obtained via the source code). Each entry is a tuple (image_name, begin_offset,
          end_offset). For example, [('7za', 0xad84, 0xae7b)].
        - ignore_priority_components (list): Configurations for the priority function in order to
          perform an ablation study of step 1. Each entry is a particular configuration of the
          priority function, and is itself a lsit containing the components of the priority
          function which must be ignored. Each entry in this inner list must be one of the following
          strings: 'caballero', 'linear-scaling', 'differential-analysis', 'randomness'.
        '''

        ALL_PRIORITY_COMPONENTS = ['caballero', 'linear-scaling', 'differential-analysis', 'randomness']

        # Check ignore_priority_components argument.
        for priority_config in ignore_priority_components:
            for entry in priority_config:
                if entry not in ALL_PRIORITY_COMPONENTS:
                    raise ValueError(f'Invalid priority component: {entry}')

        self.start_timing(f'step1_analysedata')

        # First, load all basic blocks into a pandas DataFrame.
        basic_blocks = util.get_nodes_of_type(self.app.db, 'BasicBlock')
        print(f'Loaded {len(basic_blocks)} basic blocks into a pandas DataFrame.')

        sizes = self.app.get_step_1_input_sizes()

        # Replace NaN values with 0 for number of executions.
        for prefix in [f'input_{size}_' for size in sizes] + [f'input_{sizes[0]}_noenc_']:
            basic_blocks[f'{prefix}num_executions'] = basic_blocks[f'{prefix}num_executions'].fillna(0)

        # Different priority components.
        # (a) Caballero
        def priority_caballero(entry):
            # Basic block must be annotated by Caballero.
            return 1.0 * (entry['caballero'] or False)

        # (b) Linear scaling
        def priority_linear_scaling(entry):
            # R^2 of the number of executions for different input sizes, i.e. a measure of how linear this number of executions scales with input size.
            x_labels = np.array(sizes)
            y_labels = np.array([entry[f'input_{x}_num_executions'] for x in x_labels])

            # Set entries with None to 0
            y_labels[y_labels == None] = 0

            # Calculate linear regression
            res = scipy.stats.linregress(x_labels.astype(float), y_labels.astype(float))

            # Check whether the slope is not (close to) 0, as this indicates basic blocks
            # that are executed a constant amount of times (e.g. key generation, initialisation, etc.).
            if abs(res.slope) < 1e-9:
                return 0
            else:
                return res.rvalue ** 2

        # (c) Differential analysis
        def priority_differential_analysis(entry):
            # Differential analysis: basic block needs to be executed when encryption is on, and not when encryption is off.
            return 1.0 * (entry[f'input_{sizes[0]}_num_executions'] != 0) * (entry[f'input_{sizes[0]}_noenc_num_executions'] == 0)

        # (d) High randomness
        def priority_randomness(entry):
            # Produced or consumed data must have high randomness
            # Obtain the list of instructions that are part of this basic block.
            insns = self.app.db.run_list(get_query(
                    Instruction(
                        P('image_name') == entry['image_name'],
                        P('image_offset') >= entry['image_offset_begin'],
                        P('image_offset') < entry['image_offset_end']
                    ).bind('ins')
                ))

            # Get the prefixes for all different inputs.
            input_sizes = self.app.get_step_1_input_sizes()

            randomness = 0

            # Get a randomness score for each instruction, and take the maximum of all instruction randomnesses to get the randomness of the basic block.
            for ins in insns:
                for input_size in input_sizes:
                    randomness = max(randomness, ins['ins'][f'input_{input_size}_read_values_entropy'] or 0.0)
                    randomness = max(randomness, ins['ins'][f'input_{input_size}_written_values_entropy'] or 0.0)

            return randomness

        # Calculate different priority components.
        for (key, fun) in [('priority_caballero', priority_caballero),
                           ('priority_linear-scaling', priority_linear_scaling),
                           ('priority_differential-analysis', priority_differential_analysis),
                           ('priority_randomness', priority_randomness)]:
            basic_blocks[key] = basic_blocks.apply(fun, axis = 1)

        # Keep track of return values.
        retvals = dict()

        self.stop_timing(f'step1_analysedata')

        for i, priority_config in enumerate(ignore_priority_components):
            self.start_timing(f'step1_analysedata_{",".join(priority_config)}')

            print()
            print('=' * 160)
            print(f'Using configuration {i+1}/{len(ignore_priority_components)} of priority function: ignoring components {priority_config}')
            print('=' * 160)
            print()

            # Next, evaluate a priority function for each basic block.
            def priority_function(entry):
                # Filter out libc and ld-linux.
                if ('libc.so' in entry['image_name']) or ('ld-linux.so' in entry['image_name']):
                    return -1000

                prio = 0

                for component in ALL_PRIORITY_COMPONENTS:
                    if component not in priority_config:
                        prio += entry[f'priority_{component}']

                return prio

            # Combine the different priority components.
            basic_blocks['priority'] = basic_blocks.apply(priority_function, axis = 1)

            # Sort by priority and calculate the rank.
            basic_blocks = basic_blocks.sort_values(by = ['priority'], ascending = False)
            basic_blocks['rank'] = basic_blocks['priority'].rank(ascending = False)

            rank_min = basic_blocks['priority'].rank(method = 'min', ascending = False)
            rank_max = basic_blocks['priority'].rank(method = 'max', ascending = False)

            basic_blocks['num_lower_rank'] = rank_min - 1
            basic_blocks['num_equal_rank'] = rank_max - rank_min + 1

            print('Basic blocks:')
            display(basic_blocks)

            # Print the basic blocks of minimum rank.
            min_rank_bbls = basic_blocks[basic_blocks['rank'] == basic_blocks['rank'].min()]
            print(f'{len(min_rank_bbls)} Basic blocks of minimum rank:')
            for index, bbl in min_rank_bbls.iterrows():
                marker = ''

                if not (bbl['image_name'], bbl['image_offset_begin'], bbl['image_offset_end']) in groundtruth_basic_blocks:
                    marker = '(False Positive, not part of groundtruth_basic_blocks!)'

                print(f'{index}: Basic block in image {bbl["image_name"]} at offset {hex(bbl["image_offset_begin"])} -> {hex(bbl["image_offset_end"])} (routine {bbl["routine_name"]}) has rank {bbl["rank"]} and priority {bbl["priority"]} {marker}')

            print()
            print()

            # Print the rank of some interesting basic blocks.
            print('Interesting basic blocks from groundtruth:')
            for (image, begin, end) in groundtruth_basic_blocks:
                mask = (basic_blocks['image_name'] == image) & (basic_blocks['image_offset_begin'] == begin) & (basic_blocks['image_offset_end'] == end)
                entry = basic_blocks[mask]

                assert entry.shape[0] == 1, f'Expected only one entry for (image, begin, end) = {(image, begin, end)}.'

                entry = entry.iloc[0]

                rank = entry['rank']
                priority = entry['priority']

                marker = ''

                min_rank_mask = (min_rank_bbls['image_name'] == image) & (min_rank_bbls['image_offset_begin'] == begin) & (min_rank_bbls['image_offset_end'] == end)
                if min_rank_bbls[min_rank_mask].shape[0] == 0:
                    marker = '(False Negative, not of minimal rank!)'

                print(f'Basic block {image}::{hex(begin)} -> {hex(end)} found at rank {rank} with priority {priority}. {marker}')

            # Keep track of return value
            retvals[','.join(priority_config)] = [(bbl['image_name'], bbl['image_offset_begin'], bbl['image_offset_end']) for index, bbl in min_rank_bbls.iterrows()], basic_blocks

            self.stop_timing(f'step1_analysedata_{",".join(priority_config)}')

        # Return the different values.
        if len(retvals.keys()) == 1:
            return retvals[list(retvals.keys())[0]]
        else:
            return retvals

    def step_1_localise_kdf_bbls_analyse_data(self) -> None:
        '''
        Performs an additional step compared to the original K-Hunt.
        To run  the generic deobfuscator later, we need to avoid large
        trace files that result from the large amount of iterations of
        the Key Derivation Function.
        We thus first locate the KDF by looking at basic blocks that
        are executed a lot of times (relatively speaking, at least),
        and of which this number of executions is independent from the
        program input size.
        '''
        basic_blocks = util.get_nodes_of_type(self.app.db, 'BasicBlock')

        def priority_function(entry):
            def execution_count_coeff_of_variation(entry):
                x_labels = np.array(self.app.get_step_1_input_sizes())
                y_labels = np.array([entry[f'input_{input_size}_num_executions'] for input_size in x_labels])

                # Remove entries with None
                y_labels = y_labels[y_labels != None]

                # Calculate coefficient of variation
                return scipy.stats.variation(y_labels.astype(float))

            return entry[f'input_1000_num_executions']
            # return -execution_count_coeff_of_variation(entry)

        basic_blocks['priority'] = basic_blocks.apply(priority_function, axis=1)
        basic_blocks = basic_blocks.sort_values(by = ['priority'], ascending = False)
        basic_blocks['rank'] = basic_blocks['priority'].rank(ascending = False)

        min_rank_bbls = basic_blocks[basic_blocks['rank'] <= 100]
        print(f'{len(min_rank_bbls)} Basic blocks of minimum rank:')
        for index, bbl in min_rank_bbls.iterrows():
            print(f'Basic block {bbl["image_name"]}::{bbl["routine_name"]}::{hex(bbl["image_offset_begin"])} -> {hex(bbl["image_offset_end"])} (priority {bbl["priority"]})')

    ############
    #  STEP 2  #
    ############

    def step_2_expand_basic_block_set_collect_data(self, traces: dict) -> None:
        '''
        Collects the data for the second step of the technique: expanding the set of basic blocks from Step 1.
        '''
        sizes = self.app.get_step_1_input_sizes()

        # (1) Data dependencies
        src = self.app.get_generic_deobf_calls_csv_path()
        assert src is not None
        dst = os.path.join(self.app.workspace.path, 'calls.csv')
        shutil.copyfile(src, dst)

        self.start_timing(f'step2_collectdata_datadeps_noshortcuts')
        data_deps = DataDependenciesModule(binary_params='', timeout=0, syscall_file='calls.csv',
                                           relationship_tag = 'noshortcuts',
                                           shortcuts = False,
                                           ignore_rsp = True,
                                           ignore_rip = True,
                                           ignore_rbp_stack_memory = True,
                                           recorder=traces[f'input_{sizes[0]}'])
        data_deps.run()
        self.stop_timing(f'step2_collectdata_datadeps_noshortcuts')

        self.start_timing(f'step2_collectdata_datadeps_shortcuts')
        data_deps = DataDependenciesModule(binary_params='', timeout=0, syscall_file='calls.csv',
                                           relationship_tag = 'shortcuts',
                                           shortcuts = True,
                                           ignore_rsp = True,
                                           ignore_rip = True,
                                           ignore_rbp_stack_memory = True,
                                           recorder=traces[f'input_{sizes[0]}'])
        data_deps.run()
        self.stop_timing(f'step2_collectdata_datadeps_shortcuts')

    def step_2_expand_basic_block_set_analyse_data(self, starting_set: list, max_depth: int) -> None:
        '''
        Performs the analysis for the second step of the technique: expanding the set of basic blocks from Step 1.

        Parameters:
        - starting_set (list): A list containing the basic blocks to start from. Each entry is a tuple
          (image, offset_begin, offset_end).
        - max_depth (int): The depth to use for the BFS search.

        Returns:
        - list: The expanded set of basic blocks, in the same format as 'starting_set'.
        '''

        self.start_timing(f'step2_analyse_data')

        # Create GDS graph.
        if self.app.db.run_evaluate('''CALL gds.graph.exists('data_dependencies') YIELD exists'''):
            self.app.db.run('''
                            CALL gds.graph.drop('data_dependencies')
                            ''')

        self.app.db.run('''
            CALL gds.graph.project.cypher(
                // Graph name
                'data_dependencies',

                // Node query
                'MATCH (n:Instruction)
                 RETURN id(n) AS id',

                // Relationship query
                'MATCH (n:Instruction) -[r:DEPENDS_ON]- (m:Instruction)
                 WHERE ((r.register IS NULL) OR (NOT r.register IN $blacklisted_regs)) AND (r.tag = "noshortcuts")
                 RETURN id(n) AS source, id(m) AS target',

                 // Parameters
                 {
                     parameters: { blacklisted_regs: ["rsp", "esp", "rip", "eip"] }
                 }
            )
            ''')

        # Get instructions that are a distance <= max_depth away in the data dependence graph
        # from another instruction that is part of one of the cryptographic basic blocks.
        instructions_to_inspect = set()

        for (image, offset_begin, offset_end) in starting_set:
            instructions = self.app.db.run_list(get_query(
                    Instruction(
                        P('image_name') == image,
                        P('image_offset') >= offset_begin,
                        P('image_offset') < offset_end
                    ).bind('ins')
                ))

            for ins in instructions:
                for entry in self.app.db.run_list(f'''
                    MATCH (source:Instruction {{image_name: "{ins['ins']['image_name']}", image_offset: {ins['ins']['image_offset']}}})
                    CALL gds.bfs.stream('data_dependencies', {{
                        sourceNode: id(source),
                        maxDepth: {max_depth}
                    }})
                    YIELD path
                    UNWIND [ n in nodes(path) ] AS node
                    RETURN node.image_name AS image_name, node.image_offset AS image_offset
                    '''):

                    instructions_to_inspect.add((entry['image_name'], entry['image_offset']))

        # Expand this set of instructions to basic blocks.
        basic_blocks_to_inspect = set()

        for (image, offset) in instructions_to_inspect:
            basic_blocks = self.app.db.run_list(get_query(
                    BasicBlock(
                        P('image_name') == image,
                        P('image_offset_begin') <= offset,
                        P('image_offset_end') > offset
                    ).bind('bbl')
                ))

            for bbl in basic_blocks:
                # Filter out basic blocks in libc and ld-linux.
                if ('libc.so' not in bbl['bbl']['image_name']) and ('ld-linux.so' not in bbl['bbl']['image_name']):
                    basic_blocks_to_inspect.add((bbl['bbl']['image_name'], bbl['bbl']['image_offset_begin'], bbl['bbl']['image_offset_end']))

        self.stop_timing(f'step2_analyse_data')

        return basic_blocks_to_inspect

    ############
    #  STEP 3  #
    ############

    def step_3_find_key_instruction_collect_data(self, traces: dict, basic_blocks_to_inspect: list) -> None:
        '''
        Collects the data for the third step of the technique: finding the instruction that reads the key from the basic blocks of Step 2.

        Parameters:
        - basic_blocks_to_inspect (list): The list of basic blocks to inspect to find the instruction that reads the key. Each entry
          is a tuple (image, offset_begin, offset_end). This is the output of the previous step.
        '''
        size = max(self.app.get_step_1_input_sizes())
        size_min = min(self.app.get_step_1_input_sizes())

        # (1) Instruction values
        self.start_timing('step3_collectdata_instrvalues')
        input_filename = f'input_{size}'
        instr_values = InstructionValuesModule(binary_params='', timeout=0, instruction_values_limit=10000,
                                            instruction_ranges=list(basic_blocks_to_inspect), properties_prefix=input_filename + '_', recorder=traces[input_filename])
        instr_values.run()
        self.stop_timing('step3_collectdata_instrvalues')

        # (2) System call trace
        self.start_timing('step3_collectdata_syscall')
        input_filename = f'input_{size_min}'
        syscall_trace = SysCallTraceModule(binary_params='', timeout=0, properties_prefix=input_filename + '_', recorder=traces[input_filename])
        syscall_trace.run()
        self.stop_timing('step3_collectdata_syscall')

    def step_3_find_key_instruction_analyse_data(self, basic_blocks_to_inspect: list, groundtruth_instructions: list, starting_offset: int = -1, ignore_priority_components: list = [[]], step_2_config: str = '', use_maximum_operand_priority: bool = True) -> None:
        '''
        Performs the analysis of the third step of the technique: finding the instruction that reads the key from the basic blocks of Step 2.

        Parameters:
        - basic_blocks_to_inspect (list): The list of basic blocks to inspect to find the instruction that reads the key. Each entry
          is a tuple (image, offset_begin, offset_end). This is the output of the previous step.
        - groundtruth_instructions (list): A list of instructions that read the key (i.e. the groundtruth obtained via the source code). Each entry is a tuple (image_name, image_offset).
        - starting_offset (int): The offset of the instruction to display by default when generating the link for BinaryNinja. Set to -1 (default) to not include a starting instruction.
        - ignore_priority_components (list): Configurations of the priority function in order to perform an ablation study of step 3. Each entry is a particular configuration of the priority function,
          and is itself a list containing the components of the priority function which must be ignored. Each entry in this inner list must be one of the following strings:
          'data-source', 'buffer-size', 'constant-keys', 'quasi-constant-keys', 'likely-keys-values', 'instruction-types'.
        '''
        # List of all allowed components.
        INSTRUCTION_COMPONENTS = ['data-source', 'buffer-size', 'constant-keys', 'instruction-types','quasi-constant-keys']
        OPERAND_COMPONENTS = [ 'likely-keys-values']
        ALL_PRIORITY_COMPONENTS = INSTRUCTION_COMPONENTS + OPERAND_COMPONENTS

        # Check ignore_priority_components arguments.
        for priority_config in ignore_priority_components:
            for entry in priority_config:
                if entry not in ALL_PRIORITY_COMPONENTS:
                    raise ValueError(f'Invalid priority component: {entry}')

        size = max(self.app.get_step_1_input_sizes())
        prefix = f'input_{size}'

        self.start_timing(f'step3_analysedata_{step_2_config}')

        if not hasattr(self.app, 'memoize_distance'):
            self.app.memoize_distance = {}

        def get_distance(src, dst):
            if (src, dst) in self.app.memoize_distance:
                return self.app.memoize_distance[(src, dst)]

            with GDSGraph('distance',
                            node_query='''
                                        MATCH (ins:Instruction) RETURN id(ins) AS id
                                        ''',
                            edge_query='''
                                        MATCH (i:Instruction) -[r:DEPENDS_ON]-> (j:Instruction)
                                        WHERE r.tag = "shortcuts"
                                        RETURN id(i) AS source, ID(j) AS target
                                        ''') as graph:
                try:
                    res = graph.dijkstra_source_target(
                        source_id=src,
                        target_id=dst,
                        node_labels=['*'],
                        relationship_types=['*'],
                    )

                    if len(res['totalCost']) == 0:
                        self.app.memoize_distance[(src, dst)] = float('inf')
                        return self.app.memoize_distance[(src, dst)]
                    elif len(res['totalCost']) == 1:
                        self.app.memoize_distance[(src, dst)] = res['totalCost'].item()
                        return self.app.memoize_distance[(src, dst)]
                    else:
                        assert(False)
                except:
                    self.app.memoize_distance[(src, dst)] = float('inf')
                    return self.app.memoize_distance[(src, dst)]

        # Construct ranges of values which we assume are memory addresses, and hence not relevant for finding the key.
        # These consist of three types:
        # (1) Stack addresses of the main thread: we identify these using the leading bytes. In Linux, the main thread's stack is always mapped at the end of user space, so:
        #           on x86: stack addresses are 0xff ?? ?? ??
        #           on x64: stack addresses are 0x00 00 7f ?? ?? ?? ?? ??
        # (2) Addresses in an mmap'ped region. This includes memory-backed file I/O, the stack addresses of threads other than the main thread, and malloc(..)'s larger than MMAP_THRESHOLD bytes.
        # (3) Addresses in the process's heap, identified using the brk(..) syscall.
        address_ranges = list()

        # (1)
        if self.app.workspace.binary_is64bit:
            address_ranges.append(('main_thread_stack',
                0x00007f0000000000,
                0x00007fffffffffff))
        else:
            address_ranges.append(('main_thread_stack',
                0xff000000,
                0xffffffff))

        # (2)
        mmaps = self.app.db.run_list(get_query(
            SystemCall(
                is_in(P(f'{prefix}_name'), ['mmap', 'mmap2'])
            ).bind('sys')
        ))

        for mmap in mmaps:
            lo = int(mmap['sys'][f'{prefix}_return_value'], 0)
            size = int(mmap['sys'][f'{prefix}_argument_1'], 0)

            address_ranges.append(('mmap', lo, lo + size))

        # (3)
        brks = self.app.db.run_list(get_query(
            SystemCall(
                P(f'{prefix}_name') == 'brk'
            ).bind('sys')
        ))

        min_brk = None
        max_brk = None

        for brk in brks:
            addr = int(brk['sys'][f'{prefix}_return_value'], 0)

            min_brk = min(min_brk, addr) if min_brk is not None else addr
            max_brk = max(max_brk, addr) if max_brk is not None else addr

        if (min_brk is not None) and (max_brk is not None):
            address_ranges.append(('heap', min_brk, max_brk))

        # Print all address ranges
        print('These value ranges will be assumed to be addresses, and hence ignored:')
        for (range_type, lo, hi) in address_ranges:
            print(f'{range_type:40}: {hex(lo):30} -> {hex(hi)}')

        # Load instructions in any BBL in basic_blocks_to_inspect into a pandas DataFrame.
        # Yes, this is an ugly way of doing it, but if we combine the condition using ORs in
        # a WHERE clause, Neo4j does not use the (image_name, image_offset) index for some
        # reason, so let's use a UNION query for now...
        instructions = self.app.db.run_to_df(' UNION '.join([get_query(
            Instruction(
                (P('image_name') == image_name) & (P('image_offset') >= image_offset_begin) & (P('image_offset') < image_offset_end)
            ).bind('ins')
        ) for (image_name, image_offset_begin, image_offset_end) in basic_blocks_to_inspect]
        ))

        print(f'Loaded {len(instructions)} instructions into a pandas DataFrame.')

        # Unpack dataframe in different columns
        instructions = util.unpack_dataframe(instructions)

        # Sort the instructions for determinism, as the order matters for K-Means cluster initialisation.
        instructions = instructions.sort_values(by = ['ins.image_name', 'ins.image_offset'])

        # Keep track of return values.
        retvals = dict()

        # Different priority components.

        # (e) Data source
        def priority_data_source(entry):
            sz = min(self.app.get_step_1_input_sizes())

            # Go back one step in non-shortcut data dep graph.
            srcs = self.app.db.run_list(f'''
                MATCH (ins:Instruction) -[r:DEPENDS_ON]-> (ins2)
                WHERE ins.image_name = "{entry['ins.image_name']}" AND ins.image_offset = {entry['ins.image_offset']} AND r.address IS NOT NULL AND r.tag = "noshortcuts"
                RETURN DISTINCT ins2
                                         ''')

            if len(srcs) == 0:
                srcs = self.app.db.run_list(f'''
                    MATCH (ins2:Instruction)
                    WHERE ins2.image_name = "{entry['ins.image_name']}" AND ins2.image_offset = {entry['ins.image_offset']}
                    RETURN DISTINCT ins2
                    ''')

            dsts = self.app.db.run_list(get_query(
                SystemCall(
                    P(f'input_{sz}_return_value') == f'{sz}',

                    isInvokedBy(
                        Instruction().bind('dst')
                    )
                )
            ))

            min_distance = float('inf')

            for src in srcs:
                for dst in dsts:
                    distance = get_distance(src['ins2'].id, dst['dst'].id)

                    if distance < min_distance:
                        min_distance = distance

            if (min_distance >= self.app.get_datadeps_path_length_threshold()):
                return 200
            else:
                return 0

        # (f) Buffer size
        def priority_buffer_size(entry):
            # Incorporate buffer size: "buffer size is constant as a function of the input size".
            # With buffer size, we mean the number of unique byte addresses that an instruction
            # reads from.

            x_labels = np.array(self.app.get_step_1_input_sizes())
            y_labels = np.array([entry[f'ins.input_{input_size}_num_unique_byte_addresses_read'] for input_size in x_labels])

            # Remove entries with None
            y_labels = y_labels[y_labels != None]

            # Calculate coefficient of variation
            return -scipy.stats.variation(y_labels.astype(float))

        # (g) Constant keys
        def priority_constant_keys(entry):
            # Keys are constant over different runs, so we look for instructions which read the same values for different inputs.
            union_set=set()

            prio = 0

            for input_size in self.app.get_step_1_input_sizes():
               my_entry = entry[f'ins.input_{input_size}_read_values']

               # Only consider large inputs, such that only frequently re-occurring values will be considered
               if input_size < 100000:
                  continue
               if my_entry == None:
                  continue
               my_entry = my_entry[0:-1].split(',')
               if len(my_entry)<1:
                    continue
               if len(my_entry)==1:
                    if my_entry[0]=='[':
                        continue

               # Filter out the infrequently occurring values
               my_entry1 = [x[1:] for x in my_entry if (x.find('(')>0 and int(x[x.find('(')+8:-8],10) > input_size/1000+1000)]
               # Remove the exact number of times a value occurs, as that will differ over differ input sizes
               my_entry2 = ','.join(my_entry1)
               my_entry3 = set(re.sub(r"occurs [0-9a-f]* time","",my_entry2).split(','))

               union_set=union_set.union(set(my_entry3))

            union_set=set([x for x in union_set if not x == ''])

            union_set_all = set()

            for input_size in self.app.get_step_1_input_sizes():
               my3_entry = entry[f'ins.input_{input_size}_read_values']
               # Only consider large inputs, such that only frequently re-occurring values will be considered
               if input_size < 100000:
                  continue
               if my3_entry == None:
                  continue
               my3_entry = my3_entry[0:-1].split(',')
               if len(my3_entry)<1:
                    continue
               if len(my3_entry)==1:
                    if my3_entry[0]=='[':
                        continue
               my3_entry1 = [x[1:] for x in my3_entry]
               my3_counts = [int(x[x.find('(')+8:-8],10) for x in my3_entry1]
               my3_ones = my3_counts.count(1)
               max_count = max(my3_counts)
               sum_count = sum(my3_counts)

               if (my3_ones/sum_count>0.9):
                  prio -= 10 * round(10 * my3_ones / sum_count,0)

            intersection_set=union_set;
            for input_size in self.app.get_step_1_input_sizes():
               my2_entry = entry[f'ins.input_{input_size}_read_values']

               # Only consider large inputs, such that only frequently re-occurring values will be considered
               if input_size < 100000:
                  continue
               if my2_entry == None:
                  continue
               my2_entry = my2_entry[0:-1].split(',')
               if len(my2_entry)<1:
                  continue
               if len(my2_entry)==1:
                  if my2_entry[0]=='[':
                    continue

               # Filter out the infrequently occurring values
               my2_entry1 = [x[1:] for x in my2_entry if (x.find('(')>0 and int(x[x.find('(')+8:-8],10) > input_size/1000+1000)]
               # Remove the exact number of times a value occurs, as that will differ over differ input sizes
               my2_entry2 = ','.join(my2_entry1)
               my2_entry3 = set(re.sub(r"occurs [0-9a-f]* time","",my2_entry2).split(','))

               intersection_set=intersection_set.intersection(set(my2_entry3))

            if (not len(union_set)==0):
               prio += 100 * len(intersection_set)/len(union_set)

            # We penalize isntructions that access exactly the same data when two different keys are expected to be used
            my4_entry = entry[f'ins.{prefix}_read_values']
            my4_entry_alternative = entry[f'ins.altpassphrase_{prefix}_read_values']
            if my4_entry == None or my4_entry_alternative==None:
                  return prio
            my4_entry = my4_entry[0:-1].split(',')
            my4_entry_alternative = my4_entry_alternative[0:-1].split(',')
            if len(my4_entry)<1 or len(my4_entry_alternative)<1:
                return prio
            if len(my4_entry)==1:
                if my4_entry[0]=='[':
                    return prio
            if len(my4_entry_alternative)==1:
                if my4_entry_alternative[0]=='[':
                    return prio
            my4_entry2 = set([re.sub(r"occurs [0-9a-f]* time","",x)[1:] for x in my4_entry])
            my4_entry2_alternative = set([re.sub(r"occurs [0-9a-f]* time","",x)[1:] for x in my4_entry_alternative])

            if my4_entry2 == my4_entry2_alternative:
                prio  -= 500

            return prio

        # (h) Quasi Constant keys bjorn
        def priority_quasi_constant_keys_bjorn(entry):
            # Quasi keys are constant within different runs, so we look for instructions which read the same values multiple times within one large run
            prio = 0

            for input_size in self.app.get_step_1_input_sizes():
               my_entry = entry[f'ins.input_{input_size}_read_values']

               # Only consider large inputs, such that only frequently re-occurring values will be considered
               if input_size < 100000:
                  continue
               if my_entry == None:
                  continue
               my_entry = my_entry[0:-1].split(',')
               if len(my_entry)<1:
                    continue
               if len(my_entry)==1:
                    if my_entry[0]=='[':
                        continue

               # Filter out the infrequently occurring values
               my_entry1 = [int(x[x.find('(')+8:-8],10) for x in my_entry]
               max_value = max(my_entry1)
               sum_values = sum(my_entry1)
               max_count = my_entry1.count(max_value)
               my_entry2 = [x for x in my_entry1 if not x==max_value]
               second_max_value = 0 if len(my_entry2)==0 else max(my_entry2)
               second_max_count = 0 if second_max_value==0 else my_entry2.count(second_max_value)

               if max_value <= 2:
                  continue

               prio += 10 * round(10 * max_value * max_count / sum_values, 0)

            return prio

        # (h) Quasi constant keys
        def priority_quasi_constant_keys(entry, op):
            string_of_vals = entry[f'ins.{prefix}_operand_{op}_read_values']
            list_of_vals = string_of_vals.split(',') if string_of_vals != '' else []

            return -100 * math.floor(len(set(list_of_vals)) / 100)

        # (i) Likely key values
        def priority_likely_keys_values(entry, op):
            string_of_vals = entry[f'ins.{prefix}_operand_{op}_read_values']
            list_of_vals = string_of_vals.split(',') if string_of_vals != '' else []

            # Filter out values which are memory addresses.
            def is_memory_address(val):
                # Remove the (occurs X time(s))
                my_val = val[0:val.find('(')-1]
                # Convert little-endian to big-endian
                addr = int(''.join(reversed(my_val.split(' '))), 16)
                for (range_type, lo, hi) in address_ranges:
                    if (lo <= addr) and (addr < hi):
                        return True

                return False

            def no_of_trailing_zero_bytes(val):
                # Remove the (occurs X time(s))
                my_val = val[0:val.find('(')-1]
                cur, nxt = my_val, my_val.removesuffix('00').rstrip(' ')

                while cur != nxt:
                    cur, nxt = nxt, nxt.removesuffix('00').rstrip(' ')

                return my_val.count('00') - cur.count('00')

            def max_byte_value(val):
               # Remove the (occurs X time(s))
               my_val = val[0:val.find('(')-1].split(' ')
               my_val = [int(x,16) for x in my_val]
               return max(my_val)

            def is_known_constant(val):
                # Remove the (occurs X time(s))
                my_val = val[0:val.find('(')-1]
                # Convert little-endian to big-endian
                addr = int(''.join(reversed(my_val.split(' '))), 16)
                return addr in [0xCABAE09052227808C2B2E8985A2A7000,0xCD80B1FCB0FDCC814C01307D317C4D00,0x15AABF7AC502A878D0D26D176FBDC700,0x8E1E90D1412B35FACFE474A55FBB6A00,0xA5DF7A6E142AF544B19BE18FCB503E00,0x3BF7CCC10D2ED9EF3618D415FAE22300,0x5EB7E955BC982FCDE27A93C60B712400,0xC2A163C8AB82234A69EB88400AE12900,0x040703090A0B0C020E05060F0D080180,0x030D0E0C0205080901040A060F0B0780,0x5a827999_5a827999_5a827999_5a827999]
            op_prio = 0



            for value in list_of_vals:
                # Deprioritise memory addresses
                if is_memory_address(value):
                    op_prio -= 1000

                # Deprioritise values with very low average byte values
                max_value = max_byte_value(value)
                if max_value<=16:
                    op_prio -= 1000

                if is_known_constant(value):
                    op_prio -= 1000

            # Deprioritise instructions that read values with trailing 00-bytes (like 07 00 00 00) exponentially.
            if len(list_of_vals) > 0:
                op_prio -= 100 * np.median([no_of_trailing_zero_bytes(value) for value in list_of_vals])

            return op_prio

        # (j) Instruction types
        def priority_instruction_types(entry):
            # We are not interested in control flow instructions, wide nops, or pops.
            if entry['ins.category'] in ['RET', 'CALL', 'UNCOND_BR', 'COND_BR', 'WIDENOP', 'POP']:
                return -1e6

            # Focus on some instructions categories first:
            # - LOGICAL (e.g. xor, or, and)
            # - BINARY (e.g. sub, add, cmp), but ignore cmp instructions, as they are not interesting
            # - SHIFTS (e.g. shl, shr, sar)
            # - AES (e.g. aesenc, aesenclast)
            #if (entry['ins.category'] in ['LOGICAL', 'BINARY', 'SHIFTS', 'AES']) and \
            #   (entry['ins.opcode'] not in ['cmp']):
            #    return 10
            #else:
            return 0

        # Calculate different priority components.
        # (1) Components for the instruction in its entirety.
        for (key, fun) in [('priority_data-source', priority_data_source),
                           ('priority_buffer-size', priority_buffer_size),
                           ('priority_constant-keys', priority_constant_keys),
                           ('priority_quasi-constant-keys', priority_quasi_constant_keys_bjorn),
                           ('priority_instruction-types', priority_instruction_types)]:
            instructions[key] = instructions.apply(fun, axis = 1)

        # (2) Components per operand of each instruction.
        MAX_OPERANDS = 4 # Seems to be the maximum needed now.

        # Returns True iff the operand op is a memory operand that the entry instruction reads from.
        def is_read_mem_operand(entry, op):
            num_operands = entry[f'ins.{prefix}_num_operands']
            num_operands = int(num_operands) if not math.isnan(num_operands) else 0

            assert num_operands <= MAX_OPERANDS, 'Need to increase MAX_OPERANDS!'

            if op >= num_operands:
                return False

            return entry[f'ins.{prefix}_operand_{op}_is_read'] and ('[' in entry[f'ins.{prefix}_operand_{op}_repr'])

        for (key, fun) in [('priority_quasi-constant-keys', priority_quasi_constant_keys),
                           ('priority_likely-keys-values', priority_likely_keys_values)]:
            for op in range(0, MAX_OPERANDS):
                instructions[f'{key}_op_{op}'] = instructions.apply(lambda x: fun(x, op) if is_read_mem_operand(x, op) else -1e6, axis = 1)

        self.stop_timing(f'step3_analysedata_{step_2_config}')

        for i, priority_config in enumerate(ignore_priority_components):
            self.start_timing(f'step3_analysedata_{step_2_config}_{",".join(priority_config)}')

            print()
            print('=' * 160)
            print(f'Using configuration {i+1}/{len(ignore_priority_components)} of priority function: ignoring components {priority_config}')
            print('=' * 160)
            print()

            # Priority for operand-specific heuristics.
            def operand_priority_function(entry):
                # Apply heuristics for operands.
                op_priorities = []

                for op in range(0, MAX_OPERANDS):
                    if not is_read_mem_operand(entry, op):
                        continue

                    op_prio = -1

                    for component in OPERAND_COMPONENTS:
                        if component not in priority_config:
                            op_prio += entry[f'priority_{component}_op_{op}']

                    # Operand priorities need be < 0, since we take the log for the threshold.
                    assert op_prio < 0
                    op_priorities.append(op_prio)

                return max(op_priorities) if len(op_priorities) > 0 else -1e6

            # Next, calculate a priority function for each instruction.
            def priority_function(entry, maximum_operand_priority=None):
                # Ignore instructions which do not have a memory operand that is read from.
                num_operands = entry[f'ins.{prefix}_num_operands']
                num_operands = int(num_operands) if not math.isnan(num_operands) else 0

                has_read_memory_operand = False

                for op in range(0, num_operands):
                    if ('[' in entry[f'ins.{prefix}_operand_{op}_repr']) and entry[f'ins.{prefix}_operand_{op}_is_read']:
                        has_read_memory_operand = True

                if not has_read_memory_operand:
                    return -1e6

                prio = 0

                # Apply operand priority.
                if maximum_operand_priority is not None:
                    prio += min(maximum_operand_priority, entry['priority_operand'])
                else:
                    prio += entry['priority_operand']

                # Apply heuristics for the entire instruction.
                for component in INSTRUCTION_COMPONENTS:
                    if component not in priority_config:
                        prio += entry[f'priority_{component}']

                return prio

            @ignore_warnings(category=ConvergenceWarning)
            def compute_operand_priority_cutoff(priorities: pd.Series):
                priorities = priorities.to_numpy()

                # Filter out -1e6.
                priorities = priorities[priorities > -1e6]

                # We want to cluster based on order of magnitude of number of values read, so let's take the log first.
                priorities = np.log(-priorities)

                # Reshape to 2D array
                priorities = priorities.reshape((-1, 1))

                # Train KMeans clustering on these data points.
                kmeans = sklearn.cluster.KMeans(n_clusters=2, random_state=0, n_init=10).fit(priorities)

                # Grab the average of the cluster centers (i.e. the threshold between the two clusters).
                threshold = np.mean(kmeans.cluster_centers_)

                # Convert this back from the log domain.
                threshold = -math.exp(threshold)

                return threshold

            # Compute operand priority.
            instructions['priority_operand'] = instructions.apply(operand_priority_function, axis = 1)

            # Compute operand priority cut-off
            maximum_operand_priority = compute_operand_priority_cutoff(instructions['priority_operand']) if use_maximum_operand_priority else None

            print(f'Thresholding operand priority at {maximum_operand_priority}.')

            instructions['priority'] = instructions.apply(lambda x: priority_function(x, maximum_operand_priority), axis = 1)

            # Sort by priority and calculate the rank.
            instructions = instructions.sort_values(by = ['priority'], ascending = False)
            instructions['rank'] = instructions['priority'].rank(ascending = False)

            rank_min = instructions['priority'].rank(method = 'min', ascending = False)
            rank_max = instructions['priority'].rank(method = 'max', ascending = False)

            instructions['num_lower_rank'] = rank_min - 1
            instructions['num_equal_rank'] = rank_max - rank_min + 1

            DISPLAY_MAX_ROWS, DISPLAY_MAX_COLS = 100, 50
            print(f'Instructions: (limited to the {DISPLAY_MAX_ROWS} instructions of highest priority)')
            with pd.option_context('display.max_rows', DISPLAY_MAX_ROWS, 'display.max_columns', DISPLAY_MAX_COLS):
                cols = ['ins.image_name', 'ins.image_offset'] + \
                       ['ins.routine_name'] + \
                       ['ins.DEBUG_filename', 'ins.DEBUG_line', 'ins.DEBUG_column'] + \
                       [_ for _ in instructions.columns.tolist() if _.startswith('priority')] + \
                       [_ for _ in instructions.columns.tolist() if _.endswith('rank')]

                display(instructions[cols].head(DISPLAY_MAX_ROWS))

            # Print instructions of minimal rank.
            print('Instructions of minimum rank:')
            min_rank_ins = instructions[instructions['rank'] == instructions['rank'].min()]

            for index, entry in min_rank_ins.iterrows():
                marker = ''

                if not (entry['ins.image_name'], entry['ins.image_offset']) in groundtruth_instructions:
                    marker = '(False Positive, not part of groundtruth_instructions!)'

                print(f'{index:5}: Instruction in image {entry["ins.image_name"]} at offset {hex(int(entry["ins.image_offset"]))}: {entry["ins.opcode"] or "???":12} {entry["ins.operands"] or "???":30} ({entry["ins.DEBUG_filename"]}:{entry["ins.DEBUG_line"]}) has rank {entry["rank"]} and priority {entry["priority"]} {marker}')

            print()
            print()

            # Print the rank of some interesting instructions.
            print('Interesting instructions from ground truth:')
            for (name, offset) in groundtruth_instructions:
                mask = (instructions['ins.image_name'] == name) & (instructions['ins.image_offset'] == offset)
                entry = instructions[mask]

                assert(entry.shape[0] <= 1)

                if entry.shape[0] == 1:
                    entry = entry.iloc[0]

                    rank = entry['rank']
                    priority = entry['priority']

                    opcode = entry['ins.opcode']
                    operands = entry['ins.operands']

                    marker = ''

                    min_rank_mask = (min_rank_ins['ins.image_name'] == name) & (min_rank_ins['ins.image_offset'] == offset)
                    if min_rank_ins[min_rank_mask].shape[0] == 0:
                        marker = '(False Negative, not of minimal rank!)'

                    print(f'Instruction {name}::{hex(offset)} ({opcode:12} {operands:30}) found at rank {rank} with priority {priority}. {marker}')
                else:
                    print(f'Instruction {name}::{hex(offset)} not part of list: basic block not detected in step 1+2!')

            print()
            print()

            # Keep track of return value
            retvals[','.join(priority_config)] = instructions

            self.stop_timing(f'step3_analysedata_{step_2_config}_{",".join(priority_config)}')

        # Return the different values.
        if len(retvals.keys()) == 1:
            return retvals[list(retvals.keys())[0]]
        else:
            return retvals

    ############
    #  STEP 4  #
    ############

    def step_4_print_key_analyse_data(self, lldb_module, instructions: list, max_breakpoint_hits = -1) -> None:
        '''
        Performs the analysis of the fourth step of the technique: placing a
        breakpoint on the instructions identified in step 3, and printing
        their operand to find the key itself.

        Parameters:
        - lldb_module: A reference to the global LLDB module of the framework.
        - instructions (list): The list of instructions that read (parts of)
        the key. Each entry is a tuple (image, offset, cmd).
        This is the output of the previous step, filtered by the manual effort
        by the RE. The 'cmd' item represents the command that should be run
        when a breakpoint hits that instruction.
        - max_breakpoint_hits (int): Stop after this many hit breakpoints to avoid
        cluttering the output.
        '''

        # Copy the input file to the debugger container.
        size = min(self.app.get_step_1_input_sizes())
        input_path = os.path.join(self.app.workspace.path, f'input_{size}')
        lldb_module.upload_file(input_path, f'input_{size}')

        # Copy any dependencies over.
        for file in os.listdir(self.app.workspace.path):
            if '.so' in file:
                lldb_module.upload_file(os.path.join(self.app.workspace.path, file), file)

        # Remove the output file, if it exists already.
        output_path = self.app.get_output_filename()
        lldb_module.run_shell_command(f'rm -f {output_path}')

        # Create a target for the application.
        target = lldb_module.debugger.CreateTarget(self.app.get_executable_path_lldb())
        assert(target)

        # Set the breakpoints.
        breakpoint_commands = dict()
        for (image, offset, command) in instructions:
            bp_addr = lldb_module.resolve_address(target, image, offset)
            bp = target.BreakpointCreateBySBAddress(bp_addr)

            # Remember command to run.
            breakpoint_commands[(image, offset)] = command

        print(f'Set breakpoints at {len(instructions)} instruction(s).')

        # Run the application.
        process = target.LaunchSimple(self.app.get_binary_params(f'input_{size}', perform_encryption = True).split(' '), ['LD_LIBRARY_PATH=.'], None)
        assert(process)

        ci = lldb_module.debugger.GetCommandInterpreter()
        assert(ci)

        cur_breakpoint = 0

        try:
            while process.GetState() == lldb.eStateStopped:
                if cur_breakpoint == max_breakpoint_hits:
                    break
                cur_breakpoint += 1

                thread = process.GetSelectedThread()
                assert(thread)

                frame = thread.GetFrameAtIndex(0)
                assert(frame)

                cur_ins = target.ReadInstructions(frame.GetPCAddress(), 1)[0]
                cur_addr = cur_ins.GetAddress()
                cur_image_name = cur_addr.GetModule().GetFileSpec().GetFilename()
                cur_offset = cur_addr.GetFileAddress()

                res = lldb.SBCommandReturnObject()
                command = breakpoint_commands[(cur_image_name, cur_offset)]
                ci.HandleCommand(command, res)

                if not res.Succeeded():
                    print(res)

                assert(res.Succeeded())

                cur_ins_str = str(cur_ins)

                prefix = f'{cur_ins_str:60} ==> '
                output = res.GetOutput().rstrip('\n').replace('\n', '\n' + ' ' * len(prefix))
                print(f'{prefix}{output}')

                process.Continue()

        finally:
            # print(process.GetSTDOUT(1024))
            # print(process.GetSTDERR(1024))

            # Kill the process. This is necessary to restart it later.
            process.Kill()

    ####################################
    # ABLATION STUDY STEP 1 AND STEP 2 #
    ####################################

    # Source: https://stackoverflow.com/questions/1482308/how-to-get-all-subsets-of-a-set-powerset
    @staticmethod
    def powerset(iterable):
        "powerset([1,2,3]) --> () (1,) (2,) (3,) (1,2) (1,3) (2,3) (1,2,3)"
        s = list(iterable)
        return chain.from_iterable(combinations(s, r) for r in range(len(s)+1))

    def run_ablation_step_1(self, expected_basic_blocks):
        '''
        Run the ablation study for step 1.

        Parameters:
        - expected_basic_blocks (list): The list of basic blocks to expect. See
          the groundtruth_basic_blocks argument of
          step_1_localise_crypto_bbls_analyse_data for more information on the
          format of this argument.
        '''
        # Use different priority function configurations, where we disable all
        # combinations of components.
        configurations = list(CryptoLocalisationTechnique.powerset([
            'caballero',
            'linear-scaling',
            'differential-analysis',
            'randomness'
                    ]))

        # Skip the configuration where we disable all elements.
        configurations.remove((
            'caballero',
            'linear-scaling',
            'differential-analysis',
            'randomness'
            ))

        retval = self.step_1_localise_crypto_bbls_analyse_data(
                groundtruth_basic_blocks = expected_basic_blocks,
                ignore_priority_components=configurations)

        data = defaultdict(lambda: [])

        for config in retval.keys():
            _, basic_blocks = retval[config]

            data['configuration'].append(config)

            for (image_name, image_offset_begin, image_offset_end) in expected_basic_blocks:
                entry = basic_blocks[(basic_blocks['image_name'] == image_name) & (basic_blocks['image_offset_begin'] == image_offset_begin) & (basic_blocks['image_offset_end'] == image_offset_end)]
                assert entry.shape[0] == 1, 'Expected only one entry!'

                data[f'{image_name}_{hex(image_offset_begin)}_{hex(image_offset_end)}_num_lower_rank'].append(int(entry['num_lower_rank']))
                data[f'{image_name}_{hex(image_offset_begin)}_{hex(image_offset_end)}_num_equal_rank'].append(int(entry['num_equal_rank']))
                data[f'{image_name}_{hex(image_offset_begin)}_{hex(image_offset_end)}_rank'].append(float(entry['rank']))

        pd.DataFrame(data).to_csv(os.path.join(self.app.workspace.path, 'ablation-study-step-1.csv'))

        return retval

    def run_ablation_step_2a(self, step_1_ablation_results, expected_basic_blocks,data_deps_depth=1):
        '''
        Perform the ablation study of step 2.

        Parameters:
        - step_1_ablation_results: The return value of run_ablation_step_1.
        - expected_basic_blocks (list): The list of basic blocks to expect.
          See the return value of step_2_expand_basic_block_set_analyse_data
          for more information on the format of this argument.
        - data_deps_depth (int): The depth to use for data dependencies expansion
          in step 2.
        '''

        retval = dict()
        data = defaultdict(lambda: [])

        # Iterate over all configurations of the priority function.
        for i, priority_config_key in enumerate(step_1_ablation_results.keys()):
            print()
            print('=' * 160)
            print(f'Using configuration {i+1}/{len(step_1_ablation_results.keys())} of priority function: ignoring components {priority_config_key}')
            print('=' * 160)
            print()

            _, basic_blocks = step_1_ablation_results[priority_config_key]

            # Iteratively expand set of basic blocks from step 1 under consideration, until we have all basic blocks from the groundtruth
            # as output of step 2.
            number_of_iterations_needed = 0
            all_basic_blocks_found = False

            priority_list = -np.sort(-np.unique(basic_blocks.priority))

            while not all_basic_blocks_found:
                priority_thresh = priority_list[number_of_iterations_needed]
                number_of_iterations_needed += 1

                interesting_bbls = [(bbl['image_name'], bbl['image_offset_begin'], bbl['image_offset_end']) for idx, bbl in basic_blocks[basic_blocks.priority >= priority_thresh].iterrows()]
                bbls_to_inspect = self.step_2_expand_basic_block_set_analyse_data(starting_set = interesting_bbls, max_depth=data_deps_depth)

                all_basic_blocks_found = (set(expected_basic_blocks) <= set(bbls_to_inspect))

            data['configuration'].append(priority_config_key)
            data['bbls_to_inspect_size'].append(len(bbls_to_inspect))
            data['interesting_bbls_size'].append(len(interesting_bbls))
            data['number_of_iterations_needed'].append(number_of_iterations_needed)

            retval[priority_config_key] = bbls_to_inspect

        pd.DataFrame(data).to_csv(os.path.join(self.app.workspace.path, 'ablation-study-step-2a.csv'))

        return data,retval

    def run_ablation_step_2b(self, step_2a_results, step_2a_data, expected_instructions):
        '''
        Perform the ablation study of step 2.

        Parameters:
        - step_1_ablation_results: The return value of run_ablation_step_1.
        - expected_basic_blocks (list): The list of basic blocks to expect.
          See the return value of step_2_expand_basic_block_set_analyse_data
          for more information on the format of this argument.
        - data_deps_depth (int): The depth to use for data dependencies expansion
          in step 2.
        '''

        retval = step_2a_results
        data = step_2a_data

        # Iterate over all configurations of the priority function.
        for i, priority_config_key in enumerate(data['configuration']):
            print()
            print('=' * 160)
            print(f'Using configuration {i+1}/{len(data["configuration"])} of priority function: ignoring components {priority_config_key}')
            print('=' * 160)
            print()

            bbls_to_inspect = retval[priority_config_key]

            instructions = self.step_3_find_key_instruction_analyse_data(basic_blocks_to_inspect = bbls_to_inspect, groundtruth_instructions = expected_instructions, ignore_priority_components = [[]])
            #display(instructions)

            for (image_name, image_offset) in expected_instructions:
                entry = instructions[(instructions['ins.image_name'] == image_name) & (instructions['ins.image_offset'] == image_offset)]
                assert entry.shape[0] <= 1, 'Expected at most one entry!'

                if entry.shape[0] == 1:
                    data[f'{image_name}_{hex(image_offset)}_num_lower_rank'].append(int(entry['num_lower_rank']))
                    data[f'{image_name}_{hex(image_offset)}_num_equal_rank'].append(int(entry['num_equal_rank']))
                    data[f'{image_name}_{hex(image_offset)}_rank'].append(float(entry['rank']))
                else:
                    print('Instruction not found: assuming worst case where RE needs to analyse all instructions')

                    lo = len(instructions)
                    eq = self.app.db.run_evaluate('''
                                                    MATCH (ins:Instruction)
                                                    RETURN COUNT(*)
                                                    ''') - lo

                    data[f'{image_name}_{hex(image_offset)}_num_lower_rank'].append(lo)
                    data[f'{image_name}_{hex(image_offset)}_num_equal_rank'].append(eq)
                    # E[Unif(Lo+1, Lo+Eq)]
                    data[f'{image_name}_{hex(image_offset)}_rank'].append((2.0*lo + eq + 1.0) / 2.0)

        pd.DataFrame(data).to_csv(os.path.join(self.app.workspace.path, 'ablation-study-step-2b.csv'))

        return retval

    def run_ablation_step_3(self, step_2_ablation_results, expected_instructions):
        '''
        Perform the ablation study of step 3.

        Parameters:
        - step_2_ablation_results: The return value of run_ablation_step_2.
        - expected_instructions (list): The list of instructions to expect.
          Each entry is a tuple (image_name, image_offset).
        '''
        data = defaultdict(lambda: [])

        # Use only the configuration of step 2 where we use all heuristics.
        step_2_config = ''

        configurations = list(CryptoLocalisationTechnique.powerset([
            'data-source',
            'buffer-size',
            'constant-keys',
            'quasi-constant-keys',
            'likely-keys-values',
            'instruction-types',
            ]))

        retval = self.step_3_find_key_instruction_analyse_data(
                basic_blocks_to_inspect=step_2_ablation_results[step_2_config],
                groundtruth_instructions=expected_instructions,
                ignore_priority_components=configurations,
                step_2_config=step_2_config, use_maximum_operand_priority=True)

        for step_3_config in retval.keys():
            instructions = retval[step_3_config]

            data['step_2_configuration'].append(step_2_config)
            data['step_3_configuration'].append(step_3_config)

            for (image_name, image_offset) in expected_instructions:
                entry = instructions[(instructions['ins.image_name'] == image_name) & (instructions['ins.image_offset'] == image_offset)]
                assert entry.shape[0] <= 1, 'Expected at most one entry!'

                if entry.shape[0] == 1:
                    data[f'{image_name}_{hex(image_offset)}_num_lower_rank'].append(int(entry['num_lower_rank']))
                    data[f'{image_name}_{hex(image_offset)}_num_equal_rank'].append(int(entry['num_equal_rank']))
                    data[f'{image_name}_{hex(image_offset)}_rank'].append(float(entry['rank']))
                else:
                    print('Instruction not found: assuming worst case where RE needs to analyse all instructions')

                    lo = len(instructions)
                    eq = self.app.db.run_evaluate('''
                                                    MATCH (ins:Instruction)
                                                    RETURN COUNT(*)
                                                    ''') - lo

                    data[f'{image_name}_{hex(image_offset)}_num_lower_rank'].append(lo)
                    data[f'{image_name}_{hex(image_offset)}_num_equal_rank'].append(eq)
                    # E[Unif(Lo+1, Lo+Eq)]
                    data[f'{image_name}_{hex(image_offset)}_rank'].append((2.0*lo + eq + 1.0) / 2.0)

        pd.DataFrame(data).to_csv(os.path.join(self.app.workspace.path, 'ablation-study-step-3.csv'))
