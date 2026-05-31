#include "display.hpp"

#include <iomanip>
#include <set>
#include <sstream>

namespace {

std::string yesNo(bool value) {
    return value ? "Yes" : "No";
}

std::string checkMark(bool value) {
    return value ? "✓" : "";
}

void printSeparator(std::ostream& out) {
    out << std::string(90, '-') << "\n";
}

}  // namespace

void printClassicState(std::ostream& out, const ClassicTomasulo& sim, int cycle) {
    out << "\n========== Ciclo " << cycle << " (modo classico) ==========\n";

    out << "\nInstruction status\n";
    printSeparator(out);
    out << std::left
        << std::setw(28) << "Instruction"
        << std::setw(10) << "Issue"
        << std::setw(12) << "Execute"
        << std::setw(14) << "Write result"
        << "\n";
    printSeparator(out);

    const auto& instructions = sim.getInstructions();
    const auto& status = sim.getInstructionStatus();
    for (size_t i = 0; i < instructions.size(); ++i) {
        out << std::setw(28) << instructions[i].text
            << std::setw(10) << checkMark(status[i].issued)
            << std::setw(12) << checkMark(status[i].executing)
            << std::setw(14) << checkMark(status[i].writeResult)
            << "\n";
    }

    out << "\nReservation stations\n";
    printSeparator(out);
    out << std::left
        << std::setw(8) << "Name"
        << std::setw(6) << "Busy"
        << std::setw(8) << "Op"
        << std::setw(24) << "Vj"
        << std::setw(24) << "Vk"
        << std::setw(8) << "Qj"
        << std::setw(8) << "Qk"
        << std::setw(20) << "A"
        << "\n";
    printSeparator(out);

    for (const auto& rs : sim.getReservationStations()) {
        out << std::setw(8) << rs.name
            << std::setw(6) << yesNo(rs.busy)
            << std::setw(8) << rs.op
            << std::setw(24) << rs.vj
            << std::setw(24) << rs.vk
            << std::setw(8) << rs.qj
            << std::setw(8) << rs.qk
            << std::setw(20) << rs.a
            << "\n";
    }

    out << "\nRegister status\n";
    printSeparator(out);
    static const int shownRegs[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
    out << std::setw(8) << "Field";
    for (int r : shownRegs) {
        out << std::setw(8) << ("F" + std::to_string(r));
    }
    out << "\n";
    printSeparator(out);

    out << std::setw(8) << "Qi";
    const auto& regStatus = sim.getRegisterStatus();
    for (int r : shownRegs) {
        std::string key = "F" + std::to_string(r);
        auto it = regStatus.find(key);
        out << std::setw(8) << (it != regStatus.end() ? it->second.qi : "");
    }
    out << "\n";
}

void printRobState(std::ostream& out, const RobTomasulo& sim, int cycle) {
    out << "\n========== Ciclo " << cycle << " (modo ROB) ==========\n";

    out << "\nReorder buffer\n";
    printSeparator(out);
    out << std::left
        << std::setw(6) << "Entry"
        << std::setw(6) << "Busy"
        << std::setw(28) << "Instruction"
        << std::setw(14) << "State"
        << std::setw(8) << "Dest"
        << std::setw(30) << "Value"
        << "\n";
    printSeparator(out);

    const auto& rob = sim.getRob();
    for (size_t i = sim.getRobHead(); i < rob.size(); ++i) {
        const auto& entry = rob[i];
        if (!entry.busy && entry.state != RobState::Commit && i >= static_cast<size_t>(sim.getRobHead())) {
            if (entry.number == 0) continue;
        }
        if (entry.number == 0 && !entry.busy) continue;

        std::string stateStr;
        switch (entry.state) {
            case RobState::Issue: stateStr = "Issue"; break;
            case RobState::Execute: stateStr = "Execute"; break;
            case RobState::WriteResult: stateStr = "Write result"; break;
            case RobState::Commit: stateStr = "Commit"; break;
        }

        if (i >= static_cast<size_t>(sim.getRobHead())) {
            out << std::setw(6) << entry.number
                << std::setw(6) << yesNo(entry.busy)
                << std::setw(28) << entry.instruction.text
                << std::setw(14) << stateStr
                << std::setw(8) << entry.destination
                << std::setw(30) << entry.value
                << "\n";
        }
    }

    out << "\nReservation stations\n";
    printSeparator(out);
    out << std::left
        << std::setw(8) << "Name"
        << std::setw(6) << "Busy"
        << std::setw(8) << "Op"
        << std::setw(20) << "Vj"
        << std::setw(20) << "Vk"
        << std::setw(6) << "Qj"
        << std::setw(6) << "Qk"
        << std::setw(6) << "Dest"
        << std::setw(16) << "A"
        << "\n";
    printSeparator(out);

    for (const auto& rs : sim.getReservationStations()) {
        std::string destStr = rs.dest > 0 ? ("#" + std::to_string(rs.dest)) : "";
        out << std::setw(8) << rs.name
            << std::setw(6) << yesNo(rs.busy)
            << std::setw(8) << rs.op
            << std::setw(20) << rs.vj
            << std::setw(20) << rs.vk
            << std::setw(6) << rs.qj
            << std::setw(6) << rs.qk
            << std::setw(6) << destStr
            << std::setw(16) << rs.a
            << "\n";
    }

    out << "\nFP register status\n";
    printSeparator(out);
    static const int shownRegs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10};
    out << std::setw(12) << "Field";
    for (int r : shownRegs) {
        out << std::setw(8) << ("F" + std::to_string(r));
    }
    out << "\n";
    printSeparator(out);

    out << std::setw(12) << "Reorder #";
    const auto& regStatus = sim.getRegisterStatus();
    for (int r : shownRegs) {
        std::string key = "F" + std::to_string(r);
        auto it = regStatus.find(key);
        if (it != regStatus.end() && it->second.busy) {
            out << std::setw(8) << it->second.reorderNum;
        } else {
            out << std::setw(8) << "";
        }
    }
    out << "\n";

    out << std::setw(12) << "Busy";
    for (int r : shownRegs) {
        std::string key = "F" + std::to_string(r);
        auto it = regStatus.find(key);
        out << std::setw(8) << (it != regStatus.end() ? yesNo(it->second.busy) : "No");
    }
    out << "\n";
}

void printFinalRegisters(std::ostream& out, SimMode mode,
                           const std::map<std::string, double>& fpRegs,
                           const std::map<std::string, int>& intRegs,
                           const std::vector<Instruction>& instructions) {
    out << "\n========== Resultado final ==========\n";
    out << "Modo: " << (mode == SimMode::Classic ? "classico" : "ROB") << "\n\n";

    std::set<std::string> usedFp;
    std::set<std::string> usedInt;
    for (const auto& instr : instructions) {
        if (!instr.dest.empty() && instr.dest[0] == 'F') usedFp.insert(instr.dest);
        if (!instr.src1.empty()) {
            if (instr.src1[0] == 'F') usedFp.insert(instr.src1);
            if (instr.src1[0] == 'R') usedInt.insert(instr.src1);
        }
        if (!instr.src2.empty()) {
            if (instr.src2[0] == 'F') usedFp.insert(instr.src2);
            if (instr.src2[0] == 'R') usedInt.insert(instr.src2);
        }
        if (!instr.baseReg.empty()) usedInt.insert(instr.baseReg);
    }

    out << "Registradores FP:\n";
    for (const auto& reg : usedFp) {
        auto it = fpRegs.find(reg);
        out << "  " << reg << " = ";
        if (it != fpRegs.end()) {
            out << std::fixed << std::setprecision(4) << it->second;
        } else {
            out << "0.0000";
        }
        out << "\n";
    }

    out << "\nRegistradores inteiros:\n";
    for (const auto& reg : usedInt) {
        auto it = intRegs.find(reg);
        out << "  " << reg << " = ";
        if (it != intRegs.end()) {
            out << it->second;
        } else {
            out << "0";
        }
        out << "\n";
    }
    out << "\n";
}
