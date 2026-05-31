#include "parser.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> splitCommas(const std::string& s) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        parts.push_back(trim(item));
    }
    return parts;
}

OpCode parseOpcode(const std::string& token) {
    std::string op = toUpper(token);
    if (op == "L.D" || op == "LD") return OpCode::LD;
    if (op == "S.D" || op == "SD") return OpCode::SD;
    if (op == "ADD.D" || op == "ADDD") return OpCode::ADD_D;
    if (op == "SUB.D" || op == "SUBD") return OpCode::SUB_D;
    if (op == "MUL.D" || op == "MULD") return OpCode::MUL_D;
    if (op == "DIV.D" || op == "DIVD") return OpCode::DIV_D;
    return OpCode::UNKNOWN;
}

Instruction parseInstructionLine(const std::string& line, int index) {
    std::stringstream ss(line);
    std::string opToken;
    ss >> opToken;
    OpCode opcode = parseOpcode(opToken);
    if (opcode == OpCode::UNKNOWN) {
        throw std::runtime_error("Instrucao invalida: " + line);
    }

    Instruction instr;
    instr.opcode = opcode;
    instr.text = line;
    instr.index = index;

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);
    auto parts = splitCommas(rest);

    if (opcode == OpCode::LD) {
        if (parts.size() < 2) throw std::runtime_error("L.D requer destino e endereco: " + line);
        instr.dest = toUpper(parts[0]);
        std::string addrPart = parts[1];
        size_t open = addrPart.find('(');
        size_t close = addrPart.find(')');
        if (open == std::string::npos || close == std::string::npos) {
            throw std::runtime_error("Endereco invalido em L.D: " + line);
        }
        instr.offset = std::stoi(trim(addrPart.substr(0, open)));
        instr.baseReg = toUpper(trim(addrPart.substr(open + 1, close - open - 1)));
    } else if (opcode == OpCode::SD) {
        if (parts.size() < 2) throw std::runtime_error("S.D requer origem e endereco: " + line);
        instr.src1 = toUpper(parts[0]);
        std::string addrPart = parts[1];
        size_t open = addrPart.find('(');
        size_t close = addrPart.find(')');
        if (open == std::string::npos || close == std::string::npos) {
            throw std::runtime_error("Endereco invalido em S.D: " + line);
        }
        instr.offset = std::stoi(trim(addrPart.substr(0, open)));
        instr.baseReg = toUpper(trim(addrPart.substr(open + 1, close - open - 1)));
    } else {
        if (parts.size() < 3) {
            throw std::runtime_error("Operacao FP requer 3 operandos: " + line);
        }
        instr.dest = toUpper(parts[0]);
        instr.src1 = toUpper(parts[1]);
        instr.src2 = toUpper(parts[2]);
    }

    return instr;
}

}  // namespace

SimulationConfig parseInputFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Nao foi possivel abrir o arquivo: " + filename);
    }

    SimulationConfig config;
    std::string line;
    int instrIndex = 0;

    while (std::getline(file, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        line = trim(line);
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        ss >> token;
        std::string upperToken = toUpper(token);

        if (upperToken == "MODE") {
            std::string mode;
            ss >> mode;
            mode = toUpper(mode);
            if (mode == "ROB") {
                config.mode = SimMode::Rob;
            } else {
                config.mode = SimMode::Classic;
            }
            continue;
        }

        if (upperToken == "MEM") {
            int addr;
            double value;
            ss >> addr >> value;
            config.memory[addr] = value;
            continue;
        }

        if (!token.empty() && (token[0] == 'R' || token[0] == 'r')) {
            int value;
            ss >> value;
            config.intRegs[toUpper(token)] = value;
            continue;
        }

        if (!token.empty() && (token[0] == 'F' || token[0] == 'f')) {
            double value;
            ss >> value;
            config.fpRegs[toUpper(token)] = value;
            continue;
        }

        if (upperToken == "LAT") {
            std::string latType;
            int cycles;
            ss >> latType >> cycles;
            latType = toUpper(latType);
            if (latType == "LOAD") config.latencies.load = cycles;
            else if (latType == "STORE") config.latencies.store = cycles;
            else if (latType == "ADD") config.latencies.addSub = cycles;
            else if (latType == "MUL") config.latencies.mul = cycles;
            else if (latType == "DIV") config.latencies.div = cycles;
            continue;
        }

        std::string reconstructed = line;
        config.instructions.push_back(parseInstructionLine(reconstructed, instrIndex++));
    }

    if (config.instructions.empty()) {
        throw std::runtime_error("Nenhuma instrucao encontrada no arquivo de entrada.");
    }

    return config;
}
