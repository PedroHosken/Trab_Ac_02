#include "classic_tomasulo.hpp"
#include "display.hpp"
#include "parser.hpp"
#include "rob_tomasulo.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* prog) {
    std::cerr << "Uso: " << prog << " [-m classic|rob] [-o arquivo_saida] arquivo_entrada\n";
    std::cerr << "  -m classic   Forca modo Tomasulo classico\n";
    std::cerr << "  -m rob       Forca modo Tomasulo com ROB\n";
    std::cerr << "  -o arquivo   Salva trace ciclo a ciclo em arquivo\n";
}

SimMode parseModeOverride(const std::string& modeStr) {
    if (modeStr == "rob" || modeStr == "ROB") return SimMode::Rob;
    return SimMode::Classic;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string inputFile;
    std::string outputFile;
    bool modeOverride = false;
    SimMode forcedMode = SimMode::Classic;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc) {
            modeOverride = true;
            forcedMode = parseModeOverride(argv[++i]);
        } else if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "Opcao desconhecida: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            inputFile = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Erro: arquivo de entrada nao especificado.\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        SimulationConfig config = parseInputFile(inputFile);
        if (modeOverride) {
            config.mode = forcedMode;
        }

        std::ostream* traceOut = &std::cout;
        std::ofstream fileOut;
        if (!outputFile.empty()) {
            fileOut.open(outputFile);
            if (!fileOut) {
                std::cerr << "Erro ao abrir arquivo de saida: " << outputFile << "\n";
                return 1;
            }
            traceOut = &fileOut;
        }

        if (config.mode == SimMode::Classic) {
            ClassicTomasulo sim(config);

            printClassicState(*traceOut, sim, 0);
            while (sim.step()) {
                printClassicState(*traceOut, sim, sim.getCycle());
            }
            printFinalRegisters(*traceOut, SimMode::Classic, sim.getFpRegs(),
                                sim.getIntRegs(), sim.getInstructions());

            if (!outputFile.empty()) {
                printFinalRegisters(std::cout, SimMode::Classic, sim.getFpRegs(),
                                    sim.getIntRegs(), sim.getInstructions());
            }
        } else {
            RobTomasulo sim(config);

            printRobState(*traceOut, sim, 0);
            while (sim.step()) {
                printRobState(*traceOut, sim, sim.getCycle());
            }
            printFinalRegisters(*traceOut, SimMode::Rob, sim.getFpRegs(),
                                sim.getIntRegs(), sim.getInstructions());

            if (!outputFile.empty()) {
                printFinalRegisters(std::cout, SimMode::Rob, sim.getFpRegs(),
                                    sim.getIntRegs(), sim.getInstructions());
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Erro: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
