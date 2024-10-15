/*
 * File: InstructionTrace.cpp
 * Author: Jing Que, Babak Yadegari, Willem Van Iseghem
 *
 * Copyright 2015 Arizona Board of Regents on behalf of the University of
 * Arizona. Copyright 2020 University of Ghent
 */

#include "pin.H"
#include "sde-init.H"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::ifstream;

const char kPathSeparator =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif
static const char *const SPACE = " ";

// FIXME: Test this code again on Windows to check if it fully works.
#ifdef _WIN32
typedef struct _IMAGE_DOS_HEADER { // DOS .EXE header
    unsigned short e_magic;        // Magic number
    unsigned short e_cblp;         // unsigned chars on last page of file
    unsigned short e_cp;           // Pages in file
    unsigned short e_crlc;         // Relocations
    unsigned short e_cparhdr;      // Size of header in paragraphs
    unsigned short e_minalloc;     // Minimum extra paragraphs needed
    unsigned short e_maxalloc;     // Maximum extra paragraphs needed
    unsigned short e_ss;           // Initial (relative) SS value
    unsigned short e_sp;           // Initial SP value
    unsigned short e_csum;         // Checksum
    unsigned short e_ip;           // Initial IP value
    unsigned short e_cs;           // Initial (relative) CS value
    unsigned short e_lfarlc;       // File address of relocation table
    unsigned short e_ovno;         // Overlay number
    unsigned short e_res[4];       // Reserved unsigned shorts
    unsigned short e_oemid;        // OEM identifier (for e_oeminfo)
    unsigned short e_oeminfo;      // OEM information; e_oemid specific
    unsigned short e_res2[10];     // Reserved unsigned shorts
    long e_lfanew;                 // File address of new exe header
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    unsigned short Machine;
    unsigned short NumberOfSections;
    unsigned long TimeDateStamp;
    unsigned long PointerToSymbolTable;
    unsigned long NumberOfSymbols;
    unsigned short SizeOfOptionalHeader;
    unsigned short characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    unsigned long VirtualAddress;
    unsigned long Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

typedef struct _IMAGE_OPTIONAL_HEADER {
    //
    // Standard fields.
    //

    unsigned short Magic;
    unsigned char MajorLinkerVersion;
    unsigned char MinorLinkerVersion;
    unsigned long SizeOfCode;
    unsigned long SizeOfInitializedData;
    unsigned long SizeOfUninitializedData;
    unsigned long AddressOfEntryPoint;
    unsigned long BaseOfCode;
    unsigned long BaseOfData;

    //
    // NT additional fields.
    //

    unsigned long ImageBase;
    unsigned long SectionAlignment;
    unsigned long FileAlignment;
    unsigned short MajorOperatingSystemVersion;
    unsigned short MinorOperatingSystemVersion;
    unsigned short MajorImageVersion;
    unsigned short MinorImageVersion;
    unsigned short MajorSubsystemVersion;
    unsigned short MinorSubsystemVersion;
    unsigned long Win32VersionValue;
    unsigned long SizeOfImage;
    unsigned long SizeOfHeaders;
    unsigned long CheckSum;
    unsigned short Subsystem;
    unsigned short Dllcharacteristics;
    unsigned long SizeOfStackReserve;
    unsigned long SizeOfStackCommit;
    unsigned long SizeOfHeapReserve;
    unsigned long SizeOfHeapCommit;
    unsigned long LoaderFlags;
    unsigned long NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_NT_HEADERS {
    unsigned long Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;
#endif

// ADDR -> pair<img_name, ftn_name>
std::map<ADDRINT, std::pair<std::string, std::string>> addressMapping;
// ADDR -> relative offset in image
std::map<ADDRINT, std::pair<std::string, ADDRINT>> addressTranslation;

std::ofstream traceFile;
std::ofstream mappingFile;

/**
 * Pin options.
 */
KNOB<std::string> outputFile(KNOB_MODE_WRITEONCE, "pintool", "output", "trace.out", "specify trace file name");
KNOB<bool> enableComments(KNOB_MODE_WRITEONCE, "pintool", "debug-deobf", "0",
                          "when set to true (1), prepend every line in the "
                          "trace with the exact location of the command.");
KNOB<bool> writeMapping(KNOB_MODE_WRITEONCE, "pintool", "mapping", "1",
                        "when set to true (1), create a mapping file between the relative address "
                        "of the instruction and the address of this run.");

bool bStopTrace = false;

ADDRINT OEP = 0;
ADDRINT FS = 0;
bool OEP_has_reached = false;

#define R_RAX 0
#define R_RCX 1
#define R_RDX 2
#define R_RBX 3
#define R_RSP 4
#define R_RBP 5
#define R_RSI 6
#define R_RDI 7
#define R_R8 8
#define R_R9 9
#define R_R10 10
#define R_R11 11
#define R_R12 12
#define R_R13 13
#define R_R14 14
#define R_R15 15
#define R_FS 16
#define R_XMM0_0 17 // Here we skipped the defines of R_XXM0_1 etc.
#define R_XMM1_0 19
#define R_XMM2_0 21
#define R_XMM3_0 23
#define R_XMM4_0 25
#define R_XMM5_0 27
#define R_XMM6_0 29
#define R_XMM7_0 31
#define R_XMM8_0 33
#define R_XMM9_0 35
#define R_XMM10_0 37
#define R_XMM11_0 39
#define R_XMM12_0 41
#define R_XMM13_0 43
#define R_XMM14_0 45
#define R_XMM15_0 47
#define R_XMM15_1 48

#define R_LAST R_XMM15_1
std::map<uint8_t, ADDRINT> registerValues;
bool firstWrite = true;

PIN_MUTEX writeLock;

/* Pretty-print the location of a given instruction's address in terms of its
 * binary image, section and address within that image. */
std::string InstructionLocation(ADDRINT ins) {
    if (ins == 0x0) {
        return "???";
    }
    PIN_LockClient();
    IMG image = IMG_FindByAddress(ins);
    RTN rtn = RTN_FindByAddress(ins);
    PIN_UnlockClient();
    if (!RTN_Valid(rtn)) {
        std::ostringstream os;
        os << "?rtn@0x" << std::hex << ins;
        return os.str();
    }
    PIN_LockClient();
    SEC section = RTN_Sec(RTN_FindByAddress(ins));
    ADDRINT offset = ins - IMG_LowAddress(image);
    PIN_UnlockClient();
    std::ostringstream os;
    os << IMG_Name(image) << "(" << SEC_Name(section) << ")"
       << "+0x" << std::hex << offset;
    return os.str();
}

inline void processGenericRegister(std::stringstream &buf, ADDRINT reg, uint8_t rv, const std::string &rs) {
    if (reg != registerValues[rv] || firstWrite) {
        buf << (SPACE + rs + "=") << reg;
        registerValues[rv] = reg;
    }
}

#ifdef TARGET_IA32E
// These are needed since removal of PIN_REGISTER in Pin 3.21
const UINT32 MAX_BYTES_PER_PINTOOL_WIDE_REG   = 1024;
const UINT32 MAX_WORDS_PER_PINTOOL_WIDE_REG   = (MAX_BYTES_PER_PINTOOL_WIDE_REG / 2);
const UINT32 MAX_DWORDS_PER_PINTOOL_WIDE_REG  = (MAX_WORDS_PER_PINTOOL_WIDE_REG / 2);
const UINT32 MAX_QWORDS_PER_PINTOOL_WIDE_REG  = (MAX_DWORDS_PER_PINTOOL_WIDE_REG / 2);
const UINT32 MAX_FLOATS_PER_PINTOOL_WIDE_REG  = (MAX_BYTES_PER_PINTOOL_WIDE_REG / sizeof(float));
const UINT32 MAX_DOUBLES_PER_PINTOOL_WIDE_REG = (MAX_BYTES_PER_PINTOOL_WIDE_REG / sizeof(double));

union PINTOOL_REGISTER
{
    UINT8 byte[MAX_BYTES_PER_PINTOOL_WIDE_REG];
    UINT16 word[MAX_WORDS_PER_PINTOOL_WIDE_REG];
    UINT32 dword[MAX_DWORDS_PER_PINTOOL_WIDE_REG];
    UINT64 qword[MAX_QWORDS_PER_PINTOOL_WIDE_REG];

    INT8 s_byte[MAX_BYTES_PER_PINTOOL_WIDE_REG];
    INT16 s_word[MAX_WORDS_PER_PINTOOL_WIDE_REG];
    INT32 s_dword[MAX_DWORDS_PER_PINTOOL_WIDE_REG];
    INT64 s_qword[MAX_QWORDS_PER_PINTOOL_WIDE_REG];

    FLT32 flt[MAX_FLOATS_PER_PINTOOL_WIDE_REG];
    FLT64 dbl[MAX_DOUBLES_PER_PINTOOL_WIDE_REG];
};

inline void processXMMRegister(std::stringstream &buf, const PINTOOL_REGISTER *xmm, uint8_t xmm_rv, const std::string &xmm_out) {
    if (xmm->qword[0] != registerValues[xmm_rv] || firstWrite) {
        buf << (SPACE + xmm_out + "_0=") << xmm->qword[0];
        registerValues[xmm_rv] = xmm->qword[0];
    }
    if (xmm->qword[1] != registerValues[xmm_rv + 1] || firstWrite) {
        buf << (SPACE + xmm_out + "_1=") << xmm->qword[1];
        registerValues[xmm_rv + 1] = xmm->qword[1];
    }
}

VOID writeToTrace(std::string *instructionString, ADDRINT ip, ADDRINT eip, ADDRINT eax, ADDRINT ecx, ADDRINT edx, ADDRINT ebx, ADDRINT esp, ADDRINT ebp,
                  ADDRINT esi, ADDRINT edi, ADDRINT r8, ADDRINT r9, ADDRINT r10, ADDRINT r11, ADDRINT r12, ADDRINT r13, ADDRINT r14, ADDRINT r15,
                  PINTOOL_REGISTER *xmm0, PINTOOL_REGISTER *xmm1, PINTOOL_REGISTER *xmm2, PINTOOL_REGISTER *xmm3, PINTOOL_REGISTER *xmm4, PINTOOL_REGISTER *xmm5, PINTOOL_REGISTER *xmm6,
                  PINTOOL_REGISTER *xmm7, PINTOOL_REGISTER *xmm8, PINTOOL_REGISTER *xmm9, PINTOOL_REGISTER *xmm10, PINTOOL_REGISTER *xmm11, PINTOOL_REGISTER *xmm12,
                  PINTOOL_REGISTER *xmm13, PINTOOL_REGISTER *xmm14, PINTOOL_REGISTER *xmm15) {
#else
VOID writeToTrace(std::string *instructionString, ADDRINT ip, ADDRINT eip, ADDRINT eax, ADDRINT ecx, ADDRINT edx, ADDRINT ebx, ADDRINT esp, ADDRINT ebp,
                  ADDRINT esi, ADDRINT edi) {
#endif
    if (bStopTrace) {
        return;
    }

    std::stringstream regSS{};
    if (enableComments) {
        regSS << "# " << InstructionLocation(ip) << endl;
    }
    regSS << dec << PIN_GetPid() << "\t" << PIN_GetTid() << "\t" << *instructionString << "\t" << hex;

    processGenericRegister(regSS, eax, R_RAX, "EAX");
    processGenericRegister(regSS, ecx, R_RCX, "ECX");
    processGenericRegister(regSS, edx, R_RDX, "EDX");
    processGenericRegister(regSS, ebx, R_RBX, "EBX");
    processGenericRegister(regSS, esp, R_RSP, "ESP");
    processGenericRegister(regSS, ebp, R_RBP, "EBP");
    processGenericRegister(regSS, esi, R_RSI, "ESI");
    processGenericRegister(regSS, edi, R_RDI, "EDI");
    regSS << " EIP=" << eip;

#ifdef TARGET_IA32E
    processGenericRegister(regSS, r8, R_R8, "R8");
    processGenericRegister(regSS, r9, R_R9, "R9");
    processGenericRegister(regSS, r10, R_R10, "R10");
    processGenericRegister(regSS, r11, R_R11, "R11");
    processGenericRegister(regSS, r12, R_R12, "R12");
    processGenericRegister(regSS, r13, R_R13, "R13");
    processGenericRegister(regSS, r14, R_R14, "R14");
    processGenericRegister(regSS, r15, R_R15, "R15");
    processGenericRegister(regSS, FS, R_FS, "FS");
    processXMMRegister(regSS, xmm0, R_XMM0_0, "XMM0");
    processXMMRegister(regSS, xmm1, R_XMM1_0, "XMM1");
    processXMMRegister(regSS, xmm2, R_XMM2_0, "XMM2");
    processXMMRegister(regSS, xmm3, R_XMM3_0, "XMM3");
    processXMMRegister(regSS, xmm4, R_XMM4_0, "XMM4");
    processXMMRegister(regSS, xmm5, R_XMM5_0, "XMM5");
    processXMMRegister(regSS, xmm6, R_XMM6_0, "XMM6");
    processXMMRegister(regSS, xmm7, R_XMM7_0, "XMM7");
    processXMMRegister(regSS, xmm8, R_XMM8_0, "XMM8");
    processXMMRegister(regSS, xmm9, R_XMM9_0, "XMM9");
    processXMMRegister(regSS, xmm10, R_XMM10_0, "XMM10");
    processXMMRegister(regSS, xmm11, R_XMM11_0, "XMM11");
    processXMMRegister(regSS, xmm12, R_XMM12_0, "XMM12");
    processXMMRegister(regSS, xmm13, R_XMM13_0, "XMM13");
    processXMMRegister(regSS, xmm14, R_XMM14_0, "XMM14");
    processXMMRegister(regSS, xmm15, R_XMM15_0, "XMM15");
#endif
    regSS << endl;

    PIN_MutexLock(&writeLock);
    traceFile << regSS.str();
    PIN_MutexUnlock(&writeLock);

    firstWrite = false;
}

void addMetaDataToTraceFile(const std::string &imgName) {
    traceFile << "# META: ARCH: ";
#ifdef TARGET_IA32E
    traceFile << "64";
#else
    traceFile << "32";
#endif
    traceFile << " PLATFORM: ";
#ifdef _WIN32
    traceFile << "WINDOWS";
#else
    traceFile << "LINUX";
#endif
    traceFile << " NAME: " << imgName;
    traceFile << endl;
}

std::string extractFilename(const std::string &filename) {
    const size_t lastBackslash = filename.rfind(kPathSeparator);

    if (lastBackslash == std::string::npos) {
        return filename;
    } else {
        return filename.substr(lastBackslash + 1);
    }
}

std::string getFunctionName(ADDRINT address) {
    if (addressMapping.find(address) == addressMapping.end()) {
        return SPACE;
    }
    return addressMapping[address].second;
}

std::string getImageName(ADDRINT address) {
    if (addressMapping.find(address) == addressMapping.end()) {
        // Try to find the image this address belongs to
        IMG img = IMG_FindByAddress(address);
        if (IMG_Valid(img)) {
            const std::string &name = IMG_Name(img);
            return extractFilename(name);
        }
        return "unknown";
    }
    return addressMapping[address].first;
}

std::string getInstructionBytes(const INS &ins) {
    EXCEPTION_INFO excep  = EXCEPTION_INFO();
    std::stringstream is;
    auto *buf = new uint8_t[INS_Size(ins)];
    PIN_FetchCode(buf, (void *)INS_Address(ins), INS_Size(ins), &excep);
    is << std::setfill('0');
    for (size_t i = 0; i < INS_Size(ins); i++) {
        is << std::hex << std::setw(2) << (int)buf[i] << SPACE;
    }
    delete[] buf;
    return is.str();
}

void addAddressToMapping(ADDRINT address) {
    if (addressTranslation.find(address) == addressTranslation.end()) {
        IMG img = IMG_FindByAddress(address);
        if (IMG_Valid(img)) {
            addressTranslation[address] = std::make_pair(getImageName(address), address - IMG_LowAddress(img));
        }
    }
}

void updateFS(ADDRINT fs) {
    if (fs != 0 && fs != FS) {
        FS = fs;
    }
}

VOID Instruction(INS ins, VOID *) {
    if (bStopTrace) {
        return;
    }

    ADDRINT address = INS_Address(ins);

#ifdef TARGET_IA32E
    // NOTE: since FS is somewhat unreliable to pass down below, we keep track of
    // it through multiple instructions.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)updateFS, IARG_REG_VALUE, REG_SEG_FS, IARG_END);
#endif

    if (!OEP_has_reached) {
        if (address != OEP) {
            return;
        }
        OEP_has_reached = true;
    }

    if (writeMapping.Value()) {
        addAddressToMapping(address);
    }

    std::stringstream ss;
    ss << getImageName(address) << "\t";
    ss << hex << address << "\t";
    ss << getFunctionName(address) << "\t";
    ss << getInstructionBytes(ins) << "\t";
    ss << INS_Disassemble(ins) << "\t";

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeToTrace, IARG_PTR, new std::string(ss.str()), IARG_INST_PTR, IARG_REG_VALUE, REG_INST_PTR, IARG_REG_VALUE,
                   REG_GAX, IARG_REG_VALUE, REG_GCX, IARG_REG_VALUE, REG_GDX, IARG_REG_VALUE, REG_GBX, IARG_REG_VALUE, REG_STACK_PTR, IARG_REG_VALUE, REG_GBP,
                   IARG_REG_VALUE, REG_GSI, IARG_REG_VALUE, REG_GDI,
#ifdef TARGET_IA32E
                   IARG_REG_VALUE, REG_R8, IARG_REG_VALUE, REG_R9, IARG_REG_VALUE, REG_R10, IARG_REG_VALUE, REG_R11, IARG_REG_VALUE, REG_R12, IARG_REG_VALUE,
                   REG_R13, IARG_REG_VALUE, REG_R14, IARG_REG_VALUE, REG_R15, IARG_REG_CONST_REFERENCE, REG_XMM0, IARG_REG_CONST_REFERENCE, REG_XMM1,
                   IARG_REG_CONST_REFERENCE, REG_XMM2, IARG_REG_CONST_REFERENCE, REG_XMM3, IARG_REG_CONST_REFERENCE, REG_XMM4, IARG_REG_CONST_REFERENCE,
                   REG_XMM5, IARG_REG_CONST_REFERENCE, REG_XMM6, IARG_REG_CONST_REFERENCE, REG_XMM7, IARG_REG_CONST_REFERENCE, REG_XMM8,
                   IARG_REG_CONST_REFERENCE, REG_XMM9, IARG_REG_CONST_REFERENCE, REG_XMM10, IARG_REG_CONST_REFERENCE, REG_XMM11, IARG_REG_CONST_REFERENCE,
                   REG_XMM12, IARG_REG_CONST_REFERENCE, REG_XMM13, IARG_REG_CONST_REFERENCE, REG_XMM14, IARG_REG_CONST_REFERENCE, REG_XMM15,
#endif
                   IARG_END);
}

#ifdef _WIN32
VOID BlockInputHack(CONTEXT *ctxt, ADDRINT return_ip) {
    PIN_SetContextReg(ctxt, REG_EAX, 0);

    ADDRINT esp = PIN_GetContextReg(ctxt, REG_ESP);
    PIN_SetContextReg(ctxt, REG_ESP, esp + 8);
    PIN_SetContextReg(ctxt, REG_INST_PTR, return_ip);
    PIN_ExecuteAt(ctxt);
}
#endif

VOID StopTrace() {
    if (!OEP_has_reached) {
        return;
    }
    bStopTrace = true;
}

void processImageRTNs(IMG img) {
    std::string imgName = IMG_Name(img);
    if (IMG_IsMainExecutable(img)) {
        imgName = extractFilename(imgName);
        addMetaDataToTraceFile(imgName);
    }
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = PIN_UndecorateSymbolName(RTN_Name(rtn), UNDECORATION_NAME_ONLY);
            ADDRINT address = RTN_Address(rtn);
            if (name != ".text" && name != "unnamedImageEntryPoint") {
                if (name.empty()) {
                    name = SPACE;
                }
                addressMapping[address] = std::make_pair(imgName, name);
                if (!IMG_IsMainExecutable(img)) {
                    RTN_Open(rtn);
#ifdef _WIN32
                    if (name == "BlockInput") {
                        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)BlockInputHack, IARG_CONTEXT, IARG_RETURN_IP, IARG_END);
                    } else if (name == "GetMessageW" || name == "ShowWindow") {
                        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopTrace, IARG_END);
                    }
#endif
                    RTN_Close(rtn);
                }
            }
        }
    }
}

static VOID LibcStartMainCalled(ADDRINT address_of_main) {
    cout << "main located at " << hexstr(address_of_main) << endl;
    OEP = address_of_main;
}

VOID imgInstrumentation(IMG img, VOID *) {
    if (bStopTrace) {
        return;
    }

    RTN libc_start_main_rtn = RTN_FindByName(img, "__libc_start_main");
    if (RTN_Valid(libc_start_main_rtn)) {
        cout << "OEP can be determined using __libc_start_main." << endl;
        RTN_Open(libc_start_main_rtn);
        RTN_InsertCall(libc_start_main_rtn, IPOINT_BEFORE, (AFUNPTR)LibcStartMainCalled, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
        RTN_Close(libc_start_main_rtn);
    }

    processImageRTNs(img);
}

VOID fini(INT32, VOID *) {
    traceFile.close();
    if (writeMapping.Value()) {
        cerr << "Writing mapping file..." << endl;
        mappingFile << "# Trace, Img, Relative" << endl;
        auto it = addressTranslation.begin();
        while (it != addressTranslation.end()) {
            mappingFile << hex << it->first << "," << it->second.first << "," << dec << it->second.second << endl;
            it++;
        }
        mappingFile.close();
    }
}

#ifdef _WIN32
void get_OEP(char *filename) {
    ifstream inf(filename, ifstream::binary);

    if (!inf.is_open()) {
        printf("Cannot open target file\n");
        return;
    }

    char *buf = new char[1024];
    inf.read(buf, 1024);
    IMAGE_DOS_HEADER *dos_header = (IMAGE_DOS_HEADER *)buf;
    IMAGE_NT_HEADERS *pINH = (IMAGE_NT_HEADERS *)((unsigned long)dos_header + dos_header->e_lfanew);
    OEP = pINH->OptionalHeader.AddressOfEntryPoint + pINH->OptionalHeader.ImageBase;
    // printf("EntryPoint:0x%.8x\n", OEP);

    delete[] buf;
}
#endif

/**
 * Prints usage information.
 **/
INT32 usage() {
    cerr << "This tool produces an instruction trace for use in the generic "
            "deobfuscation tool."
         << endl
         << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

void initializeRegisterValues() {
    for (uint8_t reg = 0; reg <= R_LAST; reg++) {
        registerValues[reg] = 0;
    }
}

int main(int argc, char *argv[]) {
    PIN_InitSymbols();

    // Initialise Pin and SDE.
    sde_pin_init(argc, argv);
    sde_init();

#ifdef _WIN32
    char *target_filename = {};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            target_filename = argv[i + 1];
            break;
        }
    }
    get_OEP(target_filename);
#endif

    traceFile.open(outputFile.Value().c_str());

    if (writeMapping.Value()) {
        auto mapName = outputFile.Value() + ".mapping";
        mappingFile.open(mapName.c_str());
    }

    initializeRegisterValues();

    PIN_MutexInit(&writeLock);
    INS_AddInstrumentFunction(Instruction, nullptr);
    IMG_AddInstrumentFunction(imgInstrumentation, nullptr);
    PIN_AddFiniFunction(fini, nullptr);

    // Never returns
    PIN_StartProgram();

    return 0;
}
