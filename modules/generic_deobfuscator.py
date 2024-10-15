import os
import re
import shutil
from enum import Flag
from subprocess import run
from typing import Dict, NamedTuple, Pattern, Tuple, Optional, List

from containers.generic_deobfuscation.gen_deob_runner import GenericDeobfuscationRunner
from core.core import Core
from core.workspace import Workspace
from modules.base_module import ConvenienceModule
from containers.pin.sderunner import get_tool_architecture, SDERecorder, SDEReplayer


class RelativeAddress(NamedTuple):
    imageName: str
    imageOffset: int


RelativeAddressMapping = Dict[int, RelativeAddress]


class Taint(Flag):
    NO_FORWARD_TAINT = 1 << 0
    FORWARD_TAINT = 1 << 1
    NO_BACKWARD_TAINT = 1 << 2
    BACKWARD_TAINT = 1 << 3
    FORWARD_TAINT_MEM = 1 << 4
    FORWARD_TAINT_REG = 1 << 5
    FORWARD_TAINT_FLAG = 1 << 6


class TaintStatus(NamedTuple):
    forward: Taint = Taint.NO_FORWARD_TAINT
    backward: Taint = Taint.NO_BACKWARD_TAINT


class MarkedTaint(NamedTuple):
    backward: bool = False
    forward: bool = False


class GenDeobAnnotations(NamedTuple):
    flag_forward_taint: bool = False
    forward_taint: bool = False
    backward_taint: bool = False
    cond_cf: bool = False
    deleted: bool = False
    tmp0: bool = False
    tmp1: bool = False
    tmp2: bool = False
    tmp3: bool = False
    mem_simplified: bool = False
    ignored: bool = False
    shared_mem: bool = False

    def to_csv_string(self, address: int) -> str:
        return f"{address},{self.flag_forward_taint},{self.forward_taint},{self.backward_taint},{self.cond_cf}," \
               f"{self.deleted},{self.tmp0},{self.tmp1},{self.tmp2},{self.tmp3},{self.mem_simplified},{self.ignored}," \
               f"{self.shared_mem}\n"

    @classmethod
    def csv_header(cls) -> str:
        return "Address,FlagTaint,ForwardTaint,BackwardTaint,ConditionalCF,DeletedIns,Tmp0,Tmp1,Tmp2,Tmp3," \
               "SimplifiedMem,Ignored,SharedMem\n"


def determine_taints(taint_string: str) -> MarkedTaint:
    return MarkedTaint(taint_string.find("BT") > -1, taint_string.find("FT") > -1)


def parse_annotations(changes: str) -> GenDeobAnnotations:
    return GenDeobAnnotations(
        changes.find("@") > -1,
        changes.find("FT") > -1,
        changes.find("BT") > -1,
        changes.find("CF") > -1,
        changes.find("#r") > -1,
        changes.find("+c") > -1,
        changes.find("+pp") > -1,
        changes.find("+op") > -1,
        changes.find("+d") > -1,
        changes.find("+m") > -1,
        changes.find("+e") > -1,
        changes.find("SH") > -1,
    )


class GenericDeobfuscator(ConvenienceModule):
    # Note: the disasm_ftn can be incomplete. Generic Deobfuscator trims the ftn name if it's too long.
    trace_file_re: Pattern[str] = re.compile(
        r"(?P<taint>.*) {1,5}(?P<index>\d+): \[0x(?P<address>.{12})] (?P<disasm_ftn>.+) ?"
        r"EAX:(?P<eax>.+) ECX:(?P<ecx>.+) EDX:(?P<edx>.+) EBX:(?P<ebx>.+) "
        r"ESP:(?P<esp>.+) EBP:(?P<ebp>.+) ESI:(?P<esi>.+) EDI:(?P<edi>.+) {2}"
        r"R:(?P<mem_r>.*) W:(?P<mem_w>.*)")

    def __init__(self, binary_params: str, timeout: int, recorder: Optional[SDERecorder] = None, deobf_args: Optional[List[str]] = None):
        super().__init__()
        self.binary_params = binary_params
        self.timeout = timeout
        self.pin_tool_name = 'gen-deob'
        self.pin_tool_params = f"-mapping 1 -output {self.get_trace_name()}"
        self.runner = SDEReplayer(Core().docker_client, self.binary_path, self.binary_params, self.pin_tool_name,
                                  self.pin_tool_params, get_tool_architecture(self.binary_is64bit), self.timeout,
                                  recorder)
        self.absolute_trace_path = os.path.join(self.workspace_path, self.get_trace_name())
        self.gen_deob = GenericDeobfuscationRunner(Core().docker_client, self.absolute_trace_path, deobf_args)
        self.skip_simplified = deobf_args is not None and '-T' in deobf_args

    def run(self):
        print("\n--- Generic Deobfuscator ---")

        print("\nGenDeob 1/4: Collect trace with Pin")
        self.runner.run()
        # shutil.move(os.path.join(self.workspace_path, "trace.out"), self.absolute_trace_path)

        print("\nGenDeob 2/4: Run Generic Deobfuscator")
        self.gen_deob.run()

        print("\nGenDeob 3/4: Calculate results")
        mapping = self.load_mapping()
        taint_status = self.load_taint(f"{self.binary_name}_taintStatus.csv")
        if not self.skip_simplified:
            simplified_taint_status = self.load_taint(f"{self.binary_name}_simplifiedTaintStatus.csv")
            deleted_instructions = taint_status.keys() - simplified_taint_status.keys()

        print("\nGenDeob 4/4: Import results")
        with open(os.path.join(self.workspace_path, "original_taint.csv"), "w") as fh:
            fh.write("Address,Image,Forward,Backward\n")
            for abs_address, taint in taint_status.items():
                translated = self.translate_address(mapping, abs_address)
                fh.write(f"{translated.imageOffset},{translated.imageName},{taint.forward.value},{taint.backward.value}\n")

        if not self.skip_simplified:
            with open(os.path.join(self.workspace_path, "simplified_taint.csv"), "w") as fh:
                fh.write("Address,Image,Forward,Backward\n")
                for abs_address, taint in simplified_taint_status.items():
                    translated = self.translate_address(mapping, abs_address)
                    fh.write(f"{translated.imageOffset},{translated.imageName},{taint.forward.value},{taint.backward.value}\n")

            with open(os.path.join(self.workspace_path, "deleted_instructions.csv"), "w") as fh:
                fh.write("Address,Image\n")
                for deleted_instruction in deleted_instructions:
                    translated = self.translate_address(mapping, deleted_instruction)
                    fh.write(f"{translated.imageOffset},{translated.imageName}\n")

        import_files = ['original_taint']
        if not self.skip_simplified:
            import_files.extend(['simplified_taint', 'deleted_instructions'])

        for base_file in import_files:
            file = f'{base_file}.csv'

            src = os.path.join(self.workspace_path, file)
            dest = os.path.join(Core().neo4j_import_directory, file)
            shutil.copyfile(src, dest)
            run(['chmod', 'a+rw', dest])

        db = Workspace.current.graph
        query = """
            CALL {
                LOAD CSV WITH HEADERS FROM 'file:///original_taint.csv' AS line
                MERGE (ins:Instruction {image_name: line.Image, image_offset: toInteger(line.Address)})
                SET ins.forward_taint = line.Forward, ins.backward_taint = line.Backward
            } IN TRANSACTIONS OF 1000 ROWS
            """
        db.run(query)
        if not self.skip_simplified:
            query = """
                CALL {
                    LOAD CSV WITH HEADERS FROM 'file:///simplified_taint.csv' AS line
                    MERGE (ins:Instruction {image_name: line.Image, image_offset: toInteger(line.Address)})
                    SET ins.simplified_forward_taint = line.Forward, ins.simplified_backward_taint = line.Backward
                } IN TRANSACTIONS OF 1000 ROWS
                """
            db.run(query)
            query = """
                CALL {
                    LOAD CSV WITH HEADERS FROM 'file:///deleted_instructions.csv' AS line
                    MERGE (ins:Instruction {image_name: line.Image, image_offset: toInteger(line.Address)})
                    SET ins.deleted = True
                } IN TRANSACTIONS OF 1000 ROWS
                """
            db.run(query)

    def get_trace_name(self) -> str:
        return f"{self.binary_name}_std_trace.txt"

    def process_unsimplified_trace_file(self) -> Tuple[Dict[int, MarkedTaint], Dict[int, int]]:
        taints = {}
        instructions = {}

        with open(os.path.join(self.workspace_path, "files", f"{self.binary_name}.trace"), "r") as fh:
            line_count = 0
            for line in fh:
                line_count += 1
                matches = self.trace_file_re.fullmatch(line.rstrip())
                if not matches:
                    print(f"Error! Line {line_count} in the trace did not match the regex! Skipping")
                    continue
                address = int(matches.group('address'), 16)
                # Always append to instructions
                instructions[int(matches.group('index'))] = address
                # Only append to taint if there's taint
                taint = matches.group('taint').strip()
                if len(taint) == 0:
                    continue
                taints[address] = determine_taints(taint)

        return taints, instructions

    def load_mapping(self) -> RelativeAddressMapping:
        mapping = {}

        with open(f"{self.absolute_trace_path}.mapping", "r") as fh:
            for line in fh:
                if line.startswith("#"):
                    continue
                line_parts = line.split(",")
                if len(line_parts) != 3:
                    print("\nInvalid line in trace mapping!")
                    continue
                mapping[int(line_parts[0], 16)] = RelativeAddress(line_parts[1], int(line_parts[2]))

        return mapping

    def process_simplified_trace_file(self, original_instructions: Dict[int, int]) -> Tuple[Dict[int, GenDeobAnnotations], Dict[int, int]]:
        simplified = {}
        deleted = original_instructions.copy()

        with open(os.path.join(self.workspace_path, "files", f"{self.binary_name}.simplified"), "r") as fh:
            line_count = 0
            for line in fh:
                line_count += 1
                matches = self.trace_file_re.fullmatch(line.rstrip())
                if not matches:
                    print(f"Error! Line {line_count} in the trace did not match the regex! Skipping")
                    continue
                index = int(matches.group('index'))
                del deleted[index]
                address = int(matches.group('address'), 16)
                possible_taints = matches.group('taint').strip()
                if len(possible_taints) > 0:
                    simplified[address] = parse_annotations(possible_taints)

        return simplified, deleted

    def load_taint(self, filename: str) -> Dict[int, TaintStatus]:
        result = {}
        first = True
        with open(os.path.join(self.workspace_path, "files", filename), "r") as fh:
            for line in fh:
                if first:
                    if line.rstrip() != "address,ft,bt":
                        print(f"\nFirst line ({line.rstrip()}) is not matching!")
                        break
                    first = False
                    continue
                parts = line.rstrip().split(",")
                if len(parts) != 3:
                    print("\nLine format not valid! Skipping line")
                    continue
                t = TaintStatus(Taint(int(parts[1])), Taint(int(parts[2])))
                result[int(parts[0], 16)] = t
        return result

    @staticmethod
    def translate_address(mapping: RelativeAddressMapping, abs_address: int) -> RelativeAddress:
        if abs_address in mapping:
            return mapping[abs_address]
        return RelativeAddress("unknown", -1)

