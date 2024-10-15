#include "pin.H"

#include <iostream>

#include <sys/mman.h>
#include <set>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;
using std::ofstream;

// Pin tool options
KNOB<bool> useOEP(KNOB_MODE_WRITEONCE, "pintool", "e", "1", "start counting only from entry point");
KNOB<int> startAtOffset(KNOB_MODE_WRITEONCE, "pintool", "s", "-1", "start at a certain offset within the main binary");
KNOB<std::string> outputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "results.out", "specify results file name");

static std::map<ADDRINT, uint64_t> insCounters;
static std::map<std::string, std::set<ADDRINT>> imageIns;

static bool startFromMain = false;
static bool OEP_has_reached = false;
static ADDRINT OEP = -1;
static int offset = -1;

// This function is called before every instruction is executed and prints the IP
VOID handleDynamicInstruction(ADDRINT ip) {
    if (!OEP_has_reached) {
        if (ip != OEP && startFromMain) {
            return;
        }
        OEP_has_reached = true;
        std::cout << "Start tracing!" << endl;
    }

    insCounters[ip]++;
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *)
{
    ADDRINT address = INS_Address(ins);
    insCounters[address] = 0;
    
    IMG i = IMG_FindByAddress(address);
    if (IMG_Valid(i)) {
        std::string iName = IMG_Name(i);
        if (imageIns.find(iName) == imageIns.end()) {
            imageIns.insert(std::make_pair(iName, std::set<ADDRINT>()));
        }
        imageIns[iName].insert(address);
    }

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) handleDynamicInstruction, IARG_INST_PTR, IARG_END);
}


static VOID LibcStartMainCalled(ADDRINT address_of_main) {
    cout << "main located at " << hexstr(address_of_main) << endl;
    OEP = address_of_main;
}

VOID imgInstrumentation(IMG img, VOID *) {
    if (IMG_IsMainExecutable(img) && offset != -1) {
        OEP = IMG_LowAddress(img) + offset;
        cout << "Set OEP to offset within main executable, start at " << hexstr(OEP) << endl;
    }
    RTN libc_start_main_rtn = RTN_FindByName(img, "__libc_start_main");
    if (RTN_Valid(libc_start_main_rtn) && offset == -1 && startFromMain) {
        cout << "OEP can be determined using __libc_start_main." << endl;
        RTN_Open(libc_start_main_rtn);
        RTN_InsertCall(
                libc_start_main_rtn, IPOINT_BEFORE, (AFUNPTR) LibcStartMainCalled,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                IARG_END
        );
        RTN_Close(libc_start_main_rtn);
    }
}

// This function is called when the application exits
VOID Fini(INT32, VOID *)
{
    uint8_t digits = 2;
    uint64_t factor = 10^digits;
    std::map<std::string, uint64_t> counts {};
    uint64_t allCount = 0;
    for (const auto& image : imageIns) {
        uint64_t iCount = 0;
        for (ADDRINT a : image.second) {
            iCount += insCounters[a];
        }
        allCount += iCount;
        counts[image.first] = iCount;
    }
    std::ofstream traceFile(outputFile.Value().c_str());
    if (allCount > 0) {
        for (const auto &c : counts) {
            cout << "Image " << c.first << ": " << c.second << " ("
                 << static_cast<double>(c.second * 100 * factor / allCount) / factor << " %)" << endl;
            traceFile << c.first << "," << c.second << endl;
        }
    }
    cout << "TOTAL INS: " << allCount << endl;
    traceFile << "TOTAL" << "," << allCount << endl;
    traceFile.close();
}

bool earlyFini(unsigned int, int, LEVEL_VM::CONTEXT *, bool, const LEVEL_BASE::EXCEPTION_INFO *, void *) {
    cerr << "signal caught" << endl;
    Fini(0, nullptr);
    PIN_ExitProcess(0);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool counts the amount of executed instructions as well as the division between executable and library counts.\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }
    PIN_InitSymbols();

    if (startAtOffset.Value() != -1) {
        offset = startAtOffset.Value();
        std::cout << "Set offset to start at to " << offset << endl;
    }
    startFromMain = useOEP.Value();
    if (startFromMain && offset == -1) {
        std::cout << "Start trace from main instead of start of initialization" << endl;
    }

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, nullptr);
    IMG_AddInstrumentFunction(imgInstrumentation, nullptr);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, nullptr);

    // Make sure we can cut off execution
    PIN_UnblockSignal(15, true);
    PIN_InterceptSignal(15, earlyFini, nullptr);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}