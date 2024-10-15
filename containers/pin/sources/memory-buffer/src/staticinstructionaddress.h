#ifndef STATICINSTRUCTIONADDRESS_H
#define STATICINSTRUCTIONADDRESS_H

#include <sstream>
#include <string>

#include "pin.H"

class StaticInstructionAddress {
public:
  // Creates a new StaticInstructionAddress corresponding to the given
  // instruction address.
  StaticInstructionAddress(ADDRINT address)
      : image_name("???"), section_name("???"), routine_name("???"),
        image_offset(-1), routine_offset(-1) {
    // Routine info.
    RTN routine = RTN_FindByAddress(address);
    if (!RTN_Valid(routine)) {
      return;
    }

    this->routine_name = RTN_Name(routine);
    this->routine_offset = address - RTN_Address(routine);

    // Section info.
    SEC section = RTN_Sec(routine);
    if (!SEC_Valid(section)) {
      return;
    }

    this->section_name = SEC_Name(section);

    // Image info.
    IMG image = SEC_Img(section);
    if (!IMG_Valid(image)) {
      return;
    }

    this->image_name = IMG_Name(image);
    this->image_offset = address - IMG_LowAddress(image);
  }

  // The name of the image this instruction belongs to.
  std::string image_name;

  // The name of the section this instruction belongs to.
  std::string section_name;

  // The name of the routine this instruction belongs to.
  std::string routine_name;

  // The offset of the instruction address relative to the base of the image.
  ADDRINT image_offset;

  // The offset of the instruction address relative to the base of the routine.
  ADDRINT routine_offset;
};

#endif
