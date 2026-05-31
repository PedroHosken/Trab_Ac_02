#include "rob_tomasulo.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

RobTomasulo::RobTomasulo(const SimulationConfig& config)
    : instructions_(config.instructions),
      instrStatus_(config.instructions.size()),
      fpRegs_(config.fpRegs),
      intRegs_(config.intRegs),
      memory_(config.memory),
      latencies_(config.latencies) {
    initStations();
    initRob();
    for (int i = 0; i <= 31; ++i) {
        regStatus_["F" + std::to_string(i)] = RegisterStatusRob{};
    }
}

void RobTomasulo::initStations() {
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

void RobTomasulo::initRob() {
    rob_.clear();
    rob_.reserve(std::max(16, static_cast<int>(instructions_.size()) + 4));
}

int RobTomasulo::allocateRobEntry() {
    if (static_cast<int>(rob_.size()) - robHead_ >=
        std::max(16, static_cast<int>(instructions_.size()) + 4)) {
        return -1;
    }
    rob_.push_back(RobEntry{});
    return static_cast<int>(rob_.size()) - 1;
}

RobEntry* RobTomasulo::findRobByNumber(int number) {
    for (auto& entry : rob_) {
        if (entry.number == number) return &entry;
    }
    return nullptr;
}

const RobEntry* RobTomasulo::findRobByNumber(int number) const {
    for (const auto& entry : rob_) {
        if (entry.number == number) return &entry;
    }
    return nullptr;
}

ReservationStation* RobTomasulo::findFreeStation(const std::string& type) {
    for (auto& rs : stations_) {
        if (!rs.busy) {
            if (type == "load" && (rs.name == "Load1" || rs.name == "Load2")) return &rs;
            if (type == "add" && rs.name.rfind("Add", 0) == 0) return &rs;
            if (type == "mult" && rs.name.rfind("Mult", 0) == 0) return &rs;
        }
    }
    return nullptr;
}

std::string RobTomasulo::formatAddress(int offset, const std::string& baseReg) const {
    std::ostringstream oss;
    oss << offset << " + Regs[" << baseReg << "]";
    return oss.str();
}

std::string RobTomasulo::formatMemValue(int addr) const {
    return "Mem[" + std::to_string(addr) + "]";
}

std::string RobTomasulo::formatRegValue(const std::string& reg) const {
    return "Regs[" + reg + "]";
}

double RobTomasulo::readMem(int addr) const {
    auto it = memory_.find(addr);
    if (it != memory_.end()) return it->second;
    return 0.0;
}

double RobTomasulo::parseNumericOrMem(const std::string& token) const {
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
    if (token.rfind("#", 0) == 0) {
        int num = std::stoi(token.substr(1));
        for (const auto& entry : rob_) {
            if (entry.number == num && entry.ready) return entry.numericValue;
        }
        return 0.0;
    }
    try {
        return std::stod(token);
    } catch (...) {
        return 0.0;
    }
}

void RobTomasulo::captureOperandRob(const std::string& reg, std::string& v, std::string& q) {
    auto it = regStatus_.find(reg);
    if (it != regStatus_.end() && it->second.busy) {
        const RobEntry* entry = findRobByNumber(it->second.reorderNum);
        if (entry && entry->ready) {
            std::ostringstream oss;
            oss << entry->numericValue;
            v = oss.str();
            q = "";
            return;
        }
        v = "";
        q = "#" + std::to_string(it->second.reorderNum);
        return;
    }

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

int RobTomasulo::latencyForOp(const std::string& op) const {
    if (op == "Load") return latencies_.load;
    if (op == "Store") return latencies_.store;
    if (op == "ADD" || op == "SUB") return latencies_.addSub;
    if (op == "MUL") return latencies_.mul;
    if (op == "DIV") return latencies_.div;
    return 1;
}

std::string RobTomasulo::robStateToString(RobState state) const {
    switch (state) {
        case RobState::Issue: return "Issue";
        case RobState::Execute: return "Execute";
        case RobState::WriteResult: return "Write result";
        case RobState::Commit: return "Commit";
    }
    return "";
}

void RobTomasulo::issueStage() {
    if (nextIssue_ >= static_cast<int>(instructions_.size())) return;

    int activeEntries = static_cast<int>(rob_.size()) - robHead_;
    if (activeEntries >= std::max(16, static_cast<int>(instructions_.size()))) return;

    const Instruction& instr = instructions_[nextIssue_];
    std::string stationType;
    if (instr.opcode == OpCode::LD || instr.opcode == OpCode::SD) stationType = "load";
    else if (instr.opcode == OpCode::ADD_D || instr.opcode == OpCode::SUB_D) stationType = "add";
    else stationType = "mult";

    ReservationStation* rs = findFreeStation(stationType);
    if (!rs) return;

    int robIdx = allocateRobEntry();
    if (robIdx < 0) return;

    int robNumber = nextRobNumber_++;

    rs->busy = true;
    rs->op = opcodeToString(instr.opcode);
    rs->vj = rs->vk = rs->qj = rs->qk = rs->a = "";
    rs->dest = robNumber;
    rs->executing = false;
    rs->readyToWrite = false;
    rs->cyclesLeft = 0;
    rs->instructionIndex = instr.index;

    RobEntry& entry = rob_[robIdx];
    entry = RobEntry{};
    entry.busy = true;
    entry.number = robNumber;
    entry.instruction = instr;
    entry.state = RobState::Issue;
    entry.instructionIndex = instr.index;
    entry.destination = (instr.opcode == OpCode::SD) ? "" : instr.dest;

    if (instr.opcode == OpCode::LD) {
        rs->a = formatAddress(instr.offset, instr.baseReg);
        entry.value = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
        regStatus_[instr.dest].reorderNum = robNumber;
        regStatus_[instr.dest].busy = true;
    } else if (instr.opcode == OpCode::SD) {
        rs->a = formatAddress(instr.offset, instr.baseReg);
        captureOperandRob(instr.src1, rs->vj, rs->qj);
        entry.destination = "";
        entry.value = "";
    } else {
        captureOperandRob(instr.src1, rs->vj, rs->qj);
        captureOperandRob(instr.src2, rs->vk, rs->qk);
        regStatus_[instr.dest].reorderNum = robNumber;
        regStatus_[instr.dest].busy = true;
    }

    instrStatus_[instr.index].issued = true;
    ++nextIssue_;
}

double RobTomasulo::getOperandValue(const ReservationStation& rs, bool isJ) const {
    const std::string& v = isJ ? rs.vj : rs.vk;
    return parseNumericOrMem(v);
}

double RobTomasulo::computeResult(const ReservationStation& rs) const {
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

std::string RobTomasulo::robValueDisplay(const ReservationStation& rs) const {
    const Instruction& instr = instructions_[rs.instructionIndex];
    if (rs.op == "Load") {
        return "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
    }
    if (rs.op == "Store") {
        return formatRegValue(instr.src1);
    }
    std::string jDisp = rs.vj.empty() ? rs.qj : rs.vj;
    std::string kDisp = rs.vk.empty() ? rs.qk : rs.vk;
    if (rs.op == "ADD") return jDisp + " + " + kDisp;
    if (rs.op == "SUB") return jDisp + " - " + kDisp;
    if (rs.op == "MUL") return jDisp + " × " + kDisp;
    if (rs.op == "DIV") return jDisp + " / " + kDisp;
    return std::to_string(rs.resultValue);
}

void RobTomasulo::executeStage() {
    for (auto& rs : stations_) {
        if (!rs.busy) continue;

        bool operandsReady = rs.qj.empty() && rs.qk.empty();
        if (rs.op == "Load" || rs.op == "Store") {
            operandsReady = rs.qj.empty();
        }
        if (!operandsReady) continue;

        RobEntry* entry = findRobByNumber(rs.dest);
        if (entry && entry->state == RobState::Issue) {
            entry->state = RobState::Execute;
        }

        if (!rs.executing) {
            rs.executing = true;
            rs.cyclesLeft = latencyForOp(rs.op);
            if (rs.instructionIndex >= 0) {
                instrStatus_[rs.instructionIndex].executing = true;
            }
        }

        if (rs.executing && rs.cyclesLeft > 0) {
            --rs.cyclesLeft;
        }

        if (rs.executing && rs.cyclesLeft == 0) {
            rs.readyToWrite = true;
            rs.resultValue = computeResult(rs);
            rs.resultDisplay = robValueDisplay(rs);
        }
    }
}

void RobTomasulo::broadcastCdb(ReservationStation& rs) {
    std::string tag = "#" + std::to_string(rs.dest);

    for (auto& other : stations_) {
        if (!other.busy) continue;
        if (other.qj == tag) {
            other.qj = "";
            if (rs.op == "Load") {
                const Instruction& instr = instructions_[rs.instructionIndex];
                other.vj = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
            } else {
                other.vj = tag;
            }
        }
        if (other.qk == tag) {
            other.qk = "";
            if (rs.op == "Load") {
                const Instruction& instr = instructions_[rs.instructionIndex];
                other.vk = "Mem[" + formatAddress(instr.offset, instr.baseReg) + "]";
            } else {
                other.vk = tag;
            }
        }
    }

    RobEntry* entry = findRobByNumber(rs.dest);
    if (entry) {
        entry->state = RobState::WriteResult;
        entry->ready = true;
        entry->numericValue = rs.resultValue;
        entry->value = rs.resultDisplay;
    }

    if (rs.instructionIndex >= 0) {
        instrStatus_[rs.instructionIndex].writeResult = true;
    }

    rs.busy = false;
    rs.executing = false;
    rs.readyToWrite = false;
    rs.op = rs.vj = rs.vk = rs.qj = rs.qk = rs.a = "";
    rs.instructionIndex = -1;
    rs.dest = 0;
}

void RobTomasulo::writeResultStage() {
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

void RobTomasulo::commitStage() {
    if (robHead_ >= static_cast<int>(rob_.size())) return;

    RobEntry& head = rob_[robHead_];
    if (!head.busy || head.state != RobState::WriteResult) return;

    const Instruction& instr = head.instruction;
    if (instr.opcode != OpCode::SD && !head.destination.empty()) {
        auto it = regStatus_.find(head.destination);
        if (it != regStatus_.end() && it->second.busy &&
            it->second.reorderNum == head.number) {
            fpRegs_[head.destination] = head.numericValue;
            it->second.busy = false;
            it->second.reorderNum = 0;
        }
    } else if (instr.opcode == OpCode::SD) {
        int addr = instr.offset + intRegs_.at(instr.baseReg);
        memory_[addr] = head.numericValue;
    }

    head.state = RobState::Commit;
    if (head.instructionIndex >= 0) {
        instrStatus_[head.instructionIndex].committed = true;
    }

    head.busy = false;
    ++robHead_;
}

bool RobTomasulo::allInstructionsComplete() const {
    for (const auto& st : instrStatus_) {
        if (!st.committed) return false;
    }
    return true;
}

bool RobTomasulo::step() {
    ++cycle_;
    commitStage();
    writeResultStage();
    executeStage();
    issueStage();
    return !isFinished();
}

bool RobTomasulo::isFinished() const {
    return allInstructionsComplete() && nextIssue_ >= static_cast<int>(instructions_.size());
}

void RobTomasulo::run() {
    while (step()) {}
}
