# IDAupdate, ida plugin to update idadb
from idaapi import BADADDR
from idc import AU_UNK, auto_wait, get_item_head, auto_mark_range, plan_and_wait


def run_ida():
    print("[SCRIPT] Waiting for ida to finish auto-analysis..")
    auto_wait()
    print("[SCRIPT] .. resuming script execution")

    with open("IDAupdate.txt", 'r') as fin:
        blocks = []
        for line in fin:
            # start_ea, end_ea
            tokens = line.split(",")

            # get original start of instruction at these addresses
            # so we can undefine them
            original_start = get_item_head(int(tokens[0], 16))
            original_end = get_item_head(int(tokens[1], 16))
            auto_mark_range(original_start, original_end, AU_UNK)

            # re-analyze these regions starting at the dangling start adress
            plan_and_wait(int(tokens[0], 16), BADADDR)
            blocks.append(tokens[0])

        # export analysis results
        from . import ida_plugin
        ida_plugin.run_ida()


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
