#ifndef TYPES_HPP
#define TYPES_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

enum class SimMode { Classic, Rob };

enum class OpCode {
    LD,
    SD,
    ADD_D,
    SUB_D,
    MUL_D,
    DIV_D,
    UNKNOWN
};

enum class RobState { Issue, Execute, WriteResult, Commit };

struct Instruction {
    OpCode opcode = OpCode::UNKNOWN;
    std::string dest;
    std::string src1;
    std::string src2;
    int offset = 0;
    std::string baseReg;
    std::string text;
    int index = 0;
};

struct InstructionStatus {
    bool issued = false;
    bool executing = false;
    bool writeResult = false;
    bool committed = false;
};

struct ReservationStation {
    std::string name;
    bool busy = false;
    std::string op;
    std::string vj;
    std::string vk;
    std::string qj;
    std::string qk;
    std::string a;
    int dest = 0;
    int cyclesLeft = 0;
    bool executing = false;
    bool readyToWrite = false;
    int instructionIndex = -1;
    double resultValue = 0.0;
    std::string resultDisplay;
};

struct RegisterStatusClassic {
    std::string qi;
};

struct RegisterStatusRob {
    int reorderNum = 0;
    bool busy = false;
};

struct RobEntry {
    bool busy = false;
    int number = 0;
    Instruction instruction;
    RobState state = RobState::Issue;
    std::string destination;
    std::string value;
    double numericValue = 0.0;
    bool ready = false;
    int instructionIndex = -1;
};

struct Latencies {
    int load = 1;
    int store = 1;
    int addSub = 2;
    int mul = 10;
    int div = 40;
};

struct SimulationConfig {
    SimMode mode = SimMode::Classic;
    std::map<std::string, int> intRegs;
    std::map<std::string, double> fpRegs;
    std::map<int, double> memory;
    std::vector<Instruction> instructions;
    Latencies latencies;
};

inline std::string opcodeToString(OpCode op) {
    switch (op) {
        case OpCode::LD: return "Load";
        case OpCode::SD: return "Store";
        case OpCode::ADD_D: return "ADD";
        case OpCode::SUB_D: return "SUB";
        case OpCode::MUL_D: return "MUL";
        case OpCode::DIV_D: return "DIV";
        default: return "";
    }
}

inline std::string opcodeToDisplay(OpCode op) {
    switch (op) {
        case OpCode::LD: return "L.D";
        case OpCode::SD: return "S.D";
        case OpCode::ADD_D: return "ADD.D";
        case OpCode::SUB_D: return "SUB.D";
        case OpCode::MUL_D: return "MUL.D";
        case OpCode::DIV_D: return "DIV.D";
        default: return "?";
    }
}

inline int fpRegIndex(const std::string& reg) {
    if (reg.empty() || reg[0] != 'F') return -1;
    return std::stoi(reg.substr(1));
}

inline int intRegIndex(const std::string& reg) {
    if (reg.empty() || reg[0] != 'R') return -1;
    return std::stoi(reg.substr(1));
}

#endif
