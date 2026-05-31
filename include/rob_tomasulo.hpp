#ifndef ROB_TOMASULO_HPP
#define ROB_TOMASULO_HPP

#include "types.hpp"
#include <map>
#include <string>
#include <vector>

class RobTomasulo {
public:
    explicit RobTomasulo(const SimulationConfig& config);

    bool step();
    void run();
    bool isFinished() const;

    int getCycle() const { return cycle_; }
    const std::vector<Instruction>& getInstructions() const { return instructions_; }
    const std::vector<InstructionStatus>& getInstructionStatus() const { return instrStatus_; }
    const std::vector<ReservationStation>& getReservationStations() const { return stations_; }
    const std::map<std::string, RegisterStatusRob>& getRegisterStatus() const { return regStatus_; }
    const std::vector<RobEntry>& getRob() const { return rob_; }
    const std::map<std::string, double>& getFpRegs() const { return fpRegs_; }
    const std::map<std::string, int>& getIntRegs() const { return intRegs_; }
    int getRobHead() const { return robHead_; }

private:
    void initStations();
    void initRob();
    void commitStage();
    void writeResultStage();
    void executeStage();
    void issueStage();

    int allocateRobEntry();
    RobEntry* findRobByNumber(int number);
    const RobEntry* findRobByNumber(int number) const;
    ReservationStation* findFreeStation(const std::string& type);
    void broadcastCdb(ReservationStation& rs);
    void captureOperandRob(const std::string& reg, std::string& v, std::string& q);
    std::string formatAddress(int offset, const std::string& baseReg) const;
    std::string formatMemValue(int addr) const;
    std::string formatRegValue(const std::string& reg) const;
    std::string robValueDisplay(const ReservationStation& rs) const;
    double parseNumericOrMem(const std::string& token) const;
    double readMem(int addr) const;
    double getOperandValue(const ReservationStation& rs, bool isJ) const;
    double computeResult(const ReservationStation& rs) const;
    int latencyForOp(const std::string& op) const;
    bool allInstructionsComplete() const;
    std::string robStateToString(RobState state) const;

    std::vector<Instruction> instructions_;
    std::vector<InstructionStatus> instrStatus_;
    std::vector<ReservationStation> stations_;
    std::vector<RobEntry> rob_;
    std::map<std::string, RegisterStatusRob> regStatus_;
    std::map<std::string, double> fpRegs_;
    std::map<std::string, int> intRegs_;
    std::map<int, double> memory_;
    Latencies latencies_;

    int cycle_ = 0;
    int nextIssue_ = 0;
    int robHead_ = 0;
    int nextRobNumber_ = 1;
};

#endif
