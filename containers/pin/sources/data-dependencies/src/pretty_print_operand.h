#include "pin.H"

#include <sstream>

/* Helper functions to pretty-print an operand of an instruction. */
std::string ins_operand_to_string(INS ins, UINT32 opIndex) {
  if (INS_OperandIsReg(ins, opIndex)) {
    return REG_StringShort(INS_OperandReg(ins, opIndex));
  } else if (INS_OperandIsMemory(ins, opIndex)) {
    REG base = INS_OperandMemoryBaseReg(ins, opIndex);
    REG index = INS_OperandMemoryIndexReg(ins, opIndex);
    REG segment = INS_OperandMemorySegmentReg(ins, opIndex);
    auto scale = INS_OperandMemoryScale(ins, opIndex);
    auto disp = INS_OperandMemoryDisplacement(ins, opIndex);

    std::ostringstream oss;

    // segment:[base + (index * scale) + displacement]
    oss << REG_StringShort(segment) << ":[" << REG_StringShort(base) << " + ("
        << REG_StringShort(index) << " * " << scale << ") + " << disp << "]";

    return oss.str();
  } else if (INS_OperandIsImmediate(ins, opIndex)) {
    std::ostringstream oss;
    oss << INS_OperandImmediate(ins, opIndex);

    return oss.str();
  } else {
    return "???";
  }
}
