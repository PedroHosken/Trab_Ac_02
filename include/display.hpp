#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "classic_tomasulo.hpp"
#include "rob_tomasulo.hpp"
#include <iostream>
#include <ostream>

void printClassicState(std::ostream& out, const ClassicTomasulo& sim, int cycle);
void printRobState(std::ostream& out, const RobTomasulo& sim, int cycle);
void printFinalRegisters(std::ostream& out, SimMode mode,
                           const std::map<std::string, double>& fpRegs,
                           const std::map<std::string, int>& intRegs,
                           const std::vector<Instruction>& instructions);

#endif
