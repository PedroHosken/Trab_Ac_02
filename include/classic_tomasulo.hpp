#ifndef CLASSIC_TOMASULO_HPP
#define CLASSIC_TOMASULO_HPP

#include "types.hpp"
#include <map>
#include <string>
#include <vector>

class ClassicTomasulo {
public:
    explicit ClassicTomasulo(const SimulationConfig& config);

    bool step();
    void run();
    bool isFinished() const;

    int getCycle() const { return cycle_; }
    const std::vector<Instruction>& getInstructions() const { return instructions_; }
    const std::vector<InstructionStatus>& getInstructionStatus() const { return instrStatus_; }
    const std::vector<ReservationStation>& getReservationStations() const { return stations_; }
    const std::map<std::string, RegisterStatusClassic>& getRegisterStatus() const { return regStatus_; }
    const std::map<std::string, double>& getFpRegs() const { return fpRegs_; }
    const std::map<std::string, int>& getIntRegs() const { return intRegs_; }

private:
    void initStations();
    void commitStage();
    void writeResultStage();
    void executeStage();
    void issueStage();

    ReservationStation* findFreeStation(const std::string& type);
    ReservationStation* findStationByName(const std::string& name);
    void broadcastCdb(ReservationStation& rs);
    void captureOperand(const std::string& reg, std::string& v, std::string& q);
    std::string formatAddress(int offset, const std::string& baseReg) const;
    std::string formatMemValue(int addr) const;
    std::string formatRegValue(const std::string& reg) const;
    double readMem(int addr) const;
    double parseNumericOrMem(const std::string& token) const;
    double getOperandValue(const ReservationStation& rs, bool isJ) const;
    double computeResult(const ReservationStation& rs) const;
    int latencyForOp(const std::string& op) const;
    bool allInstructionsComplete() const;

    std::vector<Instruction> instructions_;
    std::vector<InstructionStatus> instrStatus_;
    std::vector<ReservationStation> stations_;
    std::map<std::string, RegisterStatusClassic> regStatus_;
    std::map<std::string, double> fpRegs_;
    std::map<std::string, int> intRegs_;
    std::map<int, double> memory_;
    Latencies latencies_;

    int cycle_ = 0;
    int nextIssue_ = 0;
};

#endif
