#include "classic_tomasulo.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

ClassicTomasulo::ClassicTomasulo(const SimulationConfig& config)
    : instructions_(config.instructions),
      instrStatus_(config.instructions.size()),
      fpRegs_(config.fpRegs),
      intRegs_(config.intRegs),
      memory_(config.memory),
      latencies_(config.latencies) {
    initStations();
    for (int i = 0; i <= 31; ++i) {
        regStatus_["F" + std::to_string(i)] = RegisterStatusClassic{};
    }
}

void ClassicTomasulo::initStations() {
    stations_ = {
        {"Load1", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Load2", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Add1", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Add2", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Add3", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Mult1", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
        {"Mult2", false, "", "", "", "", "", "", 0, 0, false, false, -1, 0.0, ""},
    };
}

ReservationStation* ClassicTomasulo::findFreeStation(const std::string& type) {
    for (auto& rs : stations_) {
        if (!rs.busy) {
            if (type == "load" && (rs.name == "Load1" || rs.name == "Load2")) return &rs;
            if (type == "add" && rs.name.rfind("Add", 0) == 0) return &rs;
            if (type == "mult" && rs.name.rfind("Mult", 0) == 0) return &rs;
        }
    }
    return nullptr;
}

ReservationStation* ClassicTomasulo::findStationByName(const std::string& name) {
    for (auto& rs : stations_) {
        if (rs.name == name) return &rs;
    }
    return nullptr;
}

std::string ClassicTomasulo::formatAddress(int offset, const std::string& baseReg) const {
    std::ostringstream oss;
    oss << offset << " + Regs[" << baseReg << "]";
    return oss.str();
}

std::string ClassicTomasulo::formatMemValue(int addr) const {
    std::ostringstream oss;
    oss << "Mem[" << addr << "]";
    return oss.str();
}

std::string ClassicTomasulo::formatRegValue(const std::string& reg) const {
    return "Regs[" + reg + "]";
}

double ClassicTomasulo::readMem(int addr) const {
    auto it = memory_.find(addr);
    if (it != memory_.end()) return it->second;
    return 0.0;
}

double ClassicTomasulo::parseNumericOrMem(const std::string& token) const {
    if (token.empty()) return 0.0;
    if (token.rfind("Regs[", 0) == 0) {
        std::string reg = token.substr(5, token.size() - 6);
        auto it = fpRegs_.find(reg);
        if (it != fpRegs_.end()) return it->second;
        return 0.0;
    }
    if (token.rfind("Mem[", 0) == 0) {
        std::string inner = token.substr(4, token.size() - 5);
        size_t plusPos = inner.find('+');
        if (plusPos != std::string::npos) {
            int offset = std::stoi(inner.substr(0, plusPos));
            size_t rbStart = inner.find('[');
            size_t rbEnd = inner.find(']');
            std::string base = inner.substr(rbStart + 1, rbEnd - rbStart - 1);
            return readMem(offset + intRegs_.at(base));
        }
        return readMem(std::stoi(inner));
    }
    try {
        return std::stod(token);
    } catch (...) {
        return 0.0;
    }
}

void ClassicTomasulo::captureOperand(const std::string& reg, std::string& v, std::string& q) {
    auto it = regStatus_.find(reg);
    if (it != regStatus_.end() && !it->second.qi.empty()) {
        v = "";
        q = it->second.qi;
    } else {
        auto fit = fpRegs_.find(reg);
        if (fit != fpRegs_.end()) {
            std::ostringstream oss;
            oss << fit->second;
            v = oss.str();
        } else {
            v = formatRegValue(reg);
        }
        q = "";
    }
}

int ClassicTomasulo::latencyForOp(const std::string& op) const {
    if (op == "Load") return latencies_.load;
    if (op == "Store") return latencies_.store;
    if (op == "ADD" || op == "SUB") return latencies_.addSub;
    if (op == "MUL") return latencies_.mul;
    if (op == "DIV") return latencies_.div;
    return 1;
}

void ClassicTomasulo::issueStage() {
    if (nextIssue_ >= static_cast<int>(instructions_.size())) return;

    const Instruction& instr = instructions_[nextIssue_];
    std::string stationType;
    if (instr.opcode == OpCode::LD || instr.opcode == OpCode::SD) stationType = "load";
    else if (instr.opcode == OpCode::ADD_D || instr.opcode == OpCode::SUB_D) stationType = "add";
    else stationType = "mult";

    ReservationStation* rs = findFreeStation(stationType);
    if (!rs) return;

    rs->busy = true;
    rs->op = opcodeToString(instr.opcode);
    rs->vj = rs->vk = rs->qj = rs->qk = rs->a = "";
    rs->executing = false;
    rs->readyToWrite = false;
    rs->cyclesLeft = 0;
    rs->instructionIndex = instr.index;

    if (instr.opcode == OpCode::LD) {
        rs->a = formatAddress(instr.offset, instr.baseReg);
        regStatus_[instr.dest].qi = rs->name;
    } else if (instr.opcode == OpCode::SD) {
        rs->a = formatAddress(instr.offset, instr.baseReg);
        captureOperand(instr.src1, rs->vj, rs->qj);
    } else {
        captureOperand(instr.src1, rs->vj, rs->qj);
        captureOperand(instr.src2, rs->vk, rs->qk);
        regStatus_[instr.dest].qi = rs->name;
    }

    instrStatus_[instr.index].issued = true;
    ++nextIssue_;
}

double ClassicTomasulo::getOperandValue(const ReservationStation& rs, bool isJ) const {
    const std::string& v = isJ ? rs.vj : rs.vk;
    return parseNumericOrMem(v);
}

double ClassicTomasulo::computeResult(const ReservationStation& rs) const {
    const Instruction& instr = instructions_[rs.instructionIndex];
    if (rs.op == "Load") {
        int addr = instr.offset + intRegs_.at(instr.baseReg);
        return readMem(addr);
    }
    if (rs.op == "Store") {
        return getOperandValue(rs, true);
    }
    double j = getOperandValue(rs, true);
    double k = getOperandValue(rs, false);
    if (rs.op == "ADD") return j + k;
    if (rs.op == "SUB") return j - k;
    if (rs.op == "MUL") return j * k;
    if (rs.op == "DIV") return k != 0.0 ? j / k : 0.0;
    return 0.0;
}

void ClassicTomasulo::executeStage() {
    for (auto& rs : stations_) {
        if (!rs.busy) continue;

        bool operandsReady = rs.qj.empty() && rs.qk.empty();
        if (rs.op == "Load" || rs.op == "Store") {
            operandsReady = rs.qj.empty();
        }

        if (!operandsReady) continue;

        if (!rs.executing) {
            rs.executing = true;
            rs.cyclesLeft = latencyForOp(rs.op);
            if (rs.instructionIndex >= 0) {
                instrStatus_[rs.instructionIndex].executing = true;
            }
            if (rs.op == "Load") {
                const Instruction& instr = instructions_[rs.instructionIndex];
                int addr = instr.offset + intRegs_.at(instr.baseReg);
                rs.a = std::to_string(addr);
            }
        }

        if (rs.executing && rs.cyclesLeft > 0) {
            --rs.cyclesLeft;
        }

        if (rs.executing && rs.cyclesLeft == 0) {
            rs.readyToWrite = true;
            rs.resultValue = computeResult(rs);
            const Instruction& instr = instructions_[rs.instructionIndex];
            if (rs.op == "Load") {
                int addr = instr.offset + intRegs_.at(instr.baseReg);
                rs.resultDisplay = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
                (void)addr;
            } else if (rs.op == "Store") {
                rs.resultDisplay = formatRegValue(instr.src1);
            } else {
                rs.resultDisplay = std::to_string(rs.resultValue);
            }
        }
    }
}

void ClassicTomasulo::broadcastCdb(ReservationStation& rs) {
    const std::string& tag = rs.name;
    for (auto& other : stations_) {
        if (!other.busy) continue;
        if (other.qj == tag) {
            other.qj = "";
            if (rs.op == "Load") {
                const Instruction& instr = instructions_[rs.instructionIndex];
                other.vj = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
            } else {
                other.vj = rs.resultDisplay.empty() ? std::to_string(rs.resultValue) : rs.resultDisplay;
            }
        }
        if (other.qk == tag) {
            other.qk = "";
            if (rs.op == "Load") {
                const Instruction& instr = instructions_[rs.instructionIndex];
                other.vk = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
            } else {
                other.vk = rs.resultDisplay.empty() ? std::to_string(rs.resultValue) : rs.resultDisplay;
            }
        }
    }

    const Instruction& instr = instructions_[rs.instructionIndex];
    if (rs.op == "Load" || rs.op == "ADD" || rs.op == "SUB" || rs.op == "MUL" || rs.op == "DIV") {
        auto it = regStatus_.find(instr.dest);
        if (it != regStatus_.end() && it->second.qi == rs.name) {
            fpRegs_[instr.dest] = rs.resultValue;
            it->second.qi = "";
        }
    } else if (rs.op == "Store") {
        int addr = instr.offset + intRegs_.at(instr.baseReg);
        memory_[addr] = rs.resultValue;
    }

    if (rs.instructionIndex >= 0) {
        instrStatus_[rs.instructionIndex].writeResult = true;
    }

    rs.busy = false;
    rs.executing = false;
    rs.readyToWrite = false;
    rs.op = rs.vj = rs.vk = rs.qj = rs.qk = rs.a = "";
    rs.instructionIndex = -1;
}

void ClassicTomasulo::writeResultStage() {
    ReservationStation* candidate = nullptr;
    for (auto& rs : stations_) {
        if (rs.busy && rs.readyToWrite) {
            if (!candidate) candidate = &rs;
        }
    }
    if (candidate) {
        broadcastCdb(*candidate);
    }
}

bool ClassicTomasulo::allInstructionsComplete() const {
    for (const auto& st : instrStatus_) {
        if (!st.writeResult) return false;
    }
    return true;
}

bool ClassicTomasulo::step() {
    ++cycle_;
    writeResultStage();
    executeStage();
    issueStage();
    return !isFinished();
}

bool ClassicTomasulo::isFinished() const {
    return allInstructionsComplete() && nextIssue_ >= static_cast<int>(instructions_.size());
}

void ClassicTomasulo::run() {
    while (step()) {}
}
