# IDAupdate, ida plugin to patch bytes in
from idaapi import BADADDR
from idc import auto_wait, qexit, AU_UNK, AU_CODE, get_item_head, auto_mark_range, patch_byte, plan_and_wait


def run_ida():
    print("[SCRIPT] Waiting for ida to finish auto-analysis..")
    auto_wait()
    print("[SCRIPT] .. resuming script execution")

    with open("IDAupdate.txt", 'r') as fin:
        print("patching..")
        for line in fin:
            # addr, bytes
            tokens = line.split(",")

            # hex(int(tokens[0],16)
            ins_size = int(len(tokens[1]) / 2) - 1
            i = 0
            original_start = get_item_head(int(tokens[0], 16))
            auto_mark_range(original_start, original_start + ins_size, AU_UNK)
            while i < ins_size:
                patch_byte(int(tokens[0], 16) + i, int(tokens[1][2 + 2 * i:2 + 2 * i + 2], 16))
                # create_byte(int(tokens[0],16)+i,1)
                i = i + 1
            auto_mark_range(original_start, original_start + ins_size, AU_CODE)

        print("reanalyzing binary after patching..")
        # ida_auto.plan_and_wait(ida_entry.get_entry(0), BADADDR)
        plan_and_wait(0, BADADDR)

    qexit(0)


def PLUGIN_ENTRY():
    print("plug entry")
    run_ida()


# when this script is loaded as the main program (i.e., a plugin in IDA),
# execute the 'run_ida' function
if __name__ == "__main__":
    print("running as plugin")
    run_ida()
else:
    run_ida()
